#pragma once
#include "common.hpp"
#include "stack.hpp"
#include "reference.hpp"
#include <vector>

// Forward references.
//
namespace ulua
{
	struct function_result;
	namespace detail
	{
		template<typename Key, bool Raw>
		struct table_proxy;
		template<typename... Tx>
		inline function_result pcall( lua_State* L, Tx&&... args );
		template<bool Raw, typename Key>
		static table_proxy<Key, Raw> make_table_proxy( lua_State* L, stack::slot slot, bool owning, Key&& key );
	}
};

namespace ulua::detail
{
	// Lazy castable.
	//
	template<typename Ref>
	struct lazy_castable
	{
		template<typename T>
		inline bool is() const
		{
			if constexpr ( Ref::is_direct )
			{
				return stack::check<T>( ( ( Ref* ) this )->state(), ( ( Ref* ) this )->slot() );
			}
			else
			{
				( ( Ref* ) this )->push();
				bool result = stack::check<T>( ( ( Ref* ) this )->state(), stack::top_t{} );
				stack::pop_n( ( ( Ref* ) this )->state(), 1 );
				return result;
			}
		}
		template<typename T>
		inline decltype( auto ) as() const
		{
			if constexpr ( Ref::is_direct )
			{
				return stack::get<T>( ( ( Ref* ) this )->state(), ( ( Ref* ) this )->slot() );
			}
			else
			{
				( ( Ref* ) this )->push();
				return stack::pop<T>( ( ( Ref* ) this )->state() );
			}
		}
		template<typename T> inline operator T() const { return this->template as<T>(); }

		inline std::string to_string() const
		{
			( ( Ref* ) this )->push();
			std::string result = stack::to_string( ( ( Ref* ) this )->state(), stack::top_t{} );
			stack::pop_n( ( ( Ref* ) this )->state(), 1 );
			return result;
		}
	};

	// Lazy indexable.
	//
	template<typename Ref>
	struct lazy_indexable
	{
		template<typename T, bool IsRaw = false>
		inline auto at( T&& key, std::bool_constant<IsRaw> = {} ) const&
		{
			if constexpr ( Ref::is_direct )
			{
				return make_table_proxy<IsRaw>( ( ( Ref* ) this )->state(), ( ( Ref* ) this )->slot(), false, std::forward<T>( key ) );
			}
			else
			{
				( ( Ref* ) this )->push();
				return make_table_proxy<IsRaw>( ( ( Ref* ) this )->state(), stack::top_t{}, true, key );
			}
		}
		template<typename T, bool IsRaw = false>
		inline auto at( T&& key, std::bool_constant<IsRaw> = {} ) &&
		{ 
			if constexpr ( Ref::is_direct )
			{
				( ( Ref* ) this )->release();
				return make_table_proxy<IsRaw>( ( ( Ref* ) this )->state(), ( ( Ref* ) this )->slot(), ( ( Ref* ) this )->ownership_flag, std::forward<T>( key ) );
			}
			else
			{
				( ( Ref* ) this )->push();
				lua_State* state = ( ( Ref* ) this )->state();
				( ( Ref* ) this )->reset( unchecked_t{} );
				return make_table_proxy<IsRaw>( state, stack::top_t{}, true, std::forward<T>( key ) );
			}
		}
		template<typename T> inline auto operator[]( T&& key ) const& { return at( std::forward<T>( key ) ); }
		template<typename T> inline auto operator[]( T&& key ) && { return std::move( *this ).at( std::forward<T>( key ) ); }
	};

	// Lazy invocable.
	//
	template<typename Ref>
	struct lazy_invocable
	{
		template<typename... Tx> inline function_result operator()( Tx&&... args ) const &
		{ 
			( ( Ref* ) this )->push();
			return pcall( ( ( Ref* ) this )->state(), std::forward<Tx>( args )... ); 
		}
		
		template<typename... Tx> inline function_result operator()( Tx&&... args ) &&
		{ 
			( ( Ref* ) this )->push();
			lua_State* state = ( ( Ref* ) this )->state();
			( ( Ref* ) this )->reset( unchecked_t{} );
			return pcall( state, std::forward<Tx>( args )... ); 
		}

		// Dumps as byte code, returns empty vector on failure.
		//
		std::vector<uint8_t> dump_bytecode() const
		{
			std::vector<uint8_t> result = {};
			( ( Ref* ) this )->push();
			bool okay = stack::dump_function( ( ( Ref* ) this )->state(), [ & ] ( std::span<const uint8_t> data )
			{
				result.insert( result.end(), data.begin(), data.end() );
			} );
			stack::pop_n( ( ( Ref* ) this )->state(), 1 );
			if ( !okay ) result.clear();
			return result;
		}
	};

	// All the traits above.
	//
	template<typename Ref>
	struct lazy : lazy_invocable<Ref>, lazy_castable<Ref>, lazy_indexable<Ref> {};
};

namespace ulua
{
	// Any object as lazy wrapper around reference.
	//
	template<Reference Ref>
	struct basic_object : Ref, detail::lazy<basic_object<Ref>>
	{
		inline constexpr basic_object() {}
		template<typename... Tx> requires( sizeof...( Tx ) != 0 && detail::Constructible<Ref, Tx...> )
		explicit inline constexpr basic_object( Tx&&... ref ) : Ref( std::forward<Tx>( ref )... ) {}
	};
	using object = basic_object<registry_reference>;
	using stack_object = basic_object<stack_reference>;
};