#pragma once
#include "common.hpp"
#include "reference.hpp"
#include "stack.hpp"
#include "lazy.hpp"

namespace ulua
{
	// Function result.
	//
	struct function_result
	{
		lua_State* L = nullptr;
		stack::slot first = 0;
		stack::slot last = 0;
		int pcall_result = 0;

		// Construction by stack slice, no copy allowed.
		//
		inline function_result() {}
		explicit inline function_result( lua_State* L, stack::slot first, stack::slot last, int pcall_result ) : L( L ), first( first ), last( last ), pcall_result( pcall_result ) {}
		inline function_result( const function_result& ) = delete;
		inline function_result& operator=( const function_result& ) = delete;

		// State checks.
		//
		inline size_t size() const { return is_error() ? 0 : size_t( last - first ); }
		inline bool is_error() const { return pcall_result != 0; }
		inline bool is_success() const { return pcall_result == 0; }
		inline void assert() const { if ( is_error() ) detail::error( L, error() ); }
		inline explicit operator bool() const { return is_success(); }

		// Error getter.
		//
		inline const char* error() const
		{
			if ( !is_error() ) return {};
			else               return stack::get<const char*>( L, first );
		}

		// Reference getter.
		//
		stack_reference get_ref( size_t i = 0 ) const { return stack_reference{ L, stack::slot( first + i ), weak_t{} }; }

		// Value getter.
		//
		template<typename T> bool is( size_t i = 0 ) const { return std::is_same_v<T, nil_t> || ( i < size() && stack::check<T>( L, first + i ) ); }
		template<typename T> decltype( auto ) as( size_t i = 0 ) const 
		{ 
			if constexpr ( std::is_same_v<T, nil_t> )
				return T{};
			if ( i >= size() )
				detail::error( L, "Return count mismatch." );
			return stack::get<T>( L, first + i );
		}
		inline std::string to_string( size_t i ) const { return stack::to_string( L, first + i ); }
		inline std::string to_string() const
		{
			if ( is_error() )
				return error();
			size_t len = size();
			if ( !len ) return "nil";

			std::string result = "{ ";
			for ( size_t n = 0; n != len; n++ )
			{
				result += to_string( n );
				result += ", ";
			}
			result[ result.size() - 2 ] = ' ';
			result[ result.size() - 1 ] = '}';
			return result;
		}

		// Decay to result.
		//
		template<typename T>
		operator T() const
		{
			assert();

			if constexpr ( detail::is_tuple_v<T> )
			{
				return [ & ] <template<typename...> typename Tup, typename... Tx> ( std::type_identity<Tup<Tx...>> )
				{
					if ( size() < sizeof...( Tx ) )
						detail::error( L, "Return count mismatch." );
					size_t it = 0;
					return Tup<Tx...>{ as<Tx>( it++ )... };
				}( std::type_identity<T>{} );
			}
			else
			{
				return as<T>();
			}
		}

		// Remove from stack on destruction.
		//
		inline ~function_result() { stack::remove( L, first, last - first ); }
	};

	// Function invocation.
	//
	namespace detail
	{
		template<typename... Tx>
		inline function_result pcall( lua_State* L, Tx&&... args )
		{
			stack::slot bottom = stack::top( L ) - 1;

			// Push the arguments.
			//
			constexpr size_t num_args = sizeof...( Tx );
			stack::push( L, std::forward<Tx>( args )... );

			// Do the pcall.
			//
			int pcall_result = lua_pcall( L, num_args, LUA_MULTRET, 0 );
			stack::slot top = stack::top( L );

			// Return the result.
			//
			return function_result{ L, bottom + 1, top + 1, pcall_result };
		}
	};

	// Function.
	//
	template<Reference Ref>
	struct basic_function : Ref, detail::lazy_invocable<basic_function<Ref>>
	{
		// TODO: Checks?

		inline basic_function() {}
		template<typename... Tx>
		explicit inline basic_function( Tx&&... ref ) : Ref( std::forward<Tx>( ref )... ) {}
	};
	using function =       basic_function<registry_reference>;
	using stack_function = basic_function<stack_reference>;
};