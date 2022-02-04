#pragma once
#include "lua_types.hpp"
#include "state.hpp"
#include "function.hpp"

namespace ulua
{
	// Coroutine instance.
	//
	struct coroutine : state_view
	{
		// Constructed by state view / null.
		//
		constexpr coroutine( state_view thrd = nullptr ) : state_view{ thrd } {}

		// New coroutine creation, equivalent to coroutine.create.
		//
		template<typename Ref>
		static coroutine create( const Ref& f )
		{
			lua_State* L = f.state();
			lua_State* cl = lua_newthread( L );
			stack::pop_n( L, 1 );

			f.push();
			stack::xmove( L, cl, 1 );

			return { cl };
		}

		// Resumes the coroutine with all the given arguments, equivalent to coroutine.resume.
		//
		template<typename... Tx>
		int resume( Tx&&... args ) const
		{
			int nargs = stack::push( this->L, std::forward_as_tuple( std::forward<Tx>( args )... ) );
			return lua_resume( this->L, nargs );
		}

		// Returns the coroutine status.
		//
		int status() const
		{
			return lua_status( this->L );
		}

		// Checks if the given state is a coroutine or the main thread.
		//
		static bool running( lua_State* L )
		{
#if ULUA_ACCEL
			bool ismain = mainthread( G( L ) ) == L;
#else
			int ismain = lua_pushthread( L );
			stack::pop_n( L, 1 );
#endif
			return !ismain;
		}

		// Yields the coroutine with all the given arguments as results, equivalent to coroutine.yield.
		// - This function should only be called as the return expression of a C function.
		//
		template<typename... Tx>
		static int resume( lua_State* L, Tx&&... args )
		{
			int nargs = stack::push( L, std::tie( std::forward<Tx>( args )... ) );
			return lua_yield( L, nargs );
		}
	};

	template<>
	struct type_traits<coroutine>
	{
		ULUA_INLINE static int push( lua_State* L, coroutine value )
		{
#if ULUA_ACCEL
			setthreadV( L, L->top, value );
			incr_top( L );
#else
			lua_pushthread( value );
			stack::xmove( value, L, 1 );
#endif
			return 1;
		}
		ULUA_INLINE static bool check( lua_State* L, int& idx )
		{
#if ULUA_ACCEL
			return tvisthread( accel::ref( L, idx++ ) );
#else
			return lua_type( L, idx++ ) == ( int ) value_type::thread;
#endif
		}
		ULUA_INLINE static coroutine get( lua_State* L, int& idx )
		{
			int i = idx;
#if ULUA_ACCEL
			auto* tv = accel::ref( L, idx++ );
			if ( tvisthread( tv ) ) [[likely]]
				return { threadV( tv ) };
#else
			if ( auto f = lua_tothread( L, idx++ ) ) [[likely]]
				return { f };
#endif
			type_error( L, i, "coroutine" );
		}
	};
};