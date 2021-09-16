#pragma once
#include "table.hpp"
#include "reference.hpp"
#include "lazy.hpp"

namespace ulua
{
	// Declare the environment type.
	//
	template<Reference Ref>
	struct basic_environment : basic_table<Ref>
	{
		inline basic_environment() {}
		template<typename... Tx> requires( sizeof...( Tx ) != 0 && detail::Constructable<Ref, Tx...> )
		explicit inline basic_environment( Tx&&... ref ) : basic_table<Ref>( std::forward<Tx>( ref )... ) {}

		// Creating constructor.
		//
		inline basic_environment( lua_State* L, create rsvd ) : basic_table<Ref>( L, rsvd ) {}
		template<Reference RefY>
		inline basic_environment( lua_State* L, create tag, const basic_table<RefY>& fallback )
		{
			stack::create_table( L, tag );
			stack::create_table( L, reserve_records{ 1 } );
			fallback.push();
			stack::set_field( L, -2, meta::index );
			stack::set_metatable( L, -2 );
			Ref ref{ L, stack::top_t{} };
			Ref::swap( ref );
		}

		// Sets the environment of the function given.
		//
		inline void set_on( const stack_reference& fn ) const
		{
			Ref::push();
			lua_setfenv( fn.state(), fn.slot() );
		}
	};
	using environment =       basic_environment<registry_reference>;
	using stack_environment = basic_environment<stack_reference>;

	// Pseudo-type for getting the current environment.
	//
	struct this_environment : stack_environment
	{
		inline this_environment( lua_State* L, int level = 1 )
		{
			if ( level != 0 && stack::push_callstack( L, level ) )
			{
				lua_getfenv( L, -1 );
				stack::remove( L, -2 );

				stack_reference ref{ L, stack::top_t{} };
				stack_environment::swap( ref );
			}
			else
			{
				stack_reference ref{ L, LUA_ENVIRONINDEX };
				stack_environment::swap( ref );
			}
		}
	};
};