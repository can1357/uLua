#pragma once
#include "common.hpp"
#include "stack.hpp"
#include "reference.hpp"

namespace ulua
{
	struct state_view;
	struct push_count { int n; };

	namespace detail
	{
		// Applies a function an pushes the result.
		//
		template<typename Ret, typename Args, typename F>
		static int apply_closure( lua_State* L, F& func )
		{
			auto apply = [ & ] () -> Ret
			{
				using ArgsPoped = tuple_pop<Args>;
				if constexpr ( std::is_same_v<typename ArgsPoped::popped, lua_State*> || std::is_same_v<typename ArgsPoped::popped, state_view> )
					return std::apply( [ & ] <typename... Tx> ( Tx&&... args ) -> Ret { return func( L, std::forward<Tx>( args )... ); }, stack::get_all<typename ArgsPoped::type>( L, 1 ) );
				else
					return std::apply( [ & ] <typename... Tx> ( Tx&&... args ) -> Ret { return func( std::forward<Tx>( args )... ); }, stack::get_all<Args>( L, 1 ) );
			};

			if constexpr ( std::is_void_v<Ret> )
			{
				apply();
				return 0;
			}
			else
			{
				Ret result = apply();

				if constexpr ( std::is_same_v<push_count, std::decay_t<Ret>> )
				{
					return result.n;
				}
				else if constexpr ( is_tuple_v<Ret> )
				{
					stack::push_all( L, std::forward<Ret>( result ) );
					return ( int ) std::tuple_size_v<Ret>;
				}
				else
				{
					stack::push( L, std::forward<Ret>( result ) );
					return 1;
				}
			}
		}

		// Pushes a runtime closure.
		//
		template<typename F>
		static void push_closure( lua_State* L, F&& func )
		{
			using Func =   std::decay_t<F>;
			using Traits = detail::function_traits<Func>;
			using Args =   typename Traits::arguments;
			using Ret =    typename Traits::return_type;
			using C =      typename Traits::owner;
	
			cfunction_t wrapper = {};
			int upvalue_count = 0;
	
			// Stateless lambda:
			//
			if constexpr ( Traits::is_lambda && std::is_default_constructible_v<Func> && std::is_trivially_destructible_v<Func> )
			{
				wrapper = [ ] ( lua_State* L ) -> int
				{
					Func fn{};
					return apply_closure<Ret, Args>( L, fn );
				};
			}
			// Stateful lambda:
			//
			else if constexpr ( Traits::is_lambda )
			{
				upvalue_count = 1;
				new ( lua_newuserdata( L, sizeof( Func ) ) ) Func( std::forward<F>( func ) );
	
				wrapper = [ ] ( lua_State* L ) -> int
				{
					auto* fn = ( Func* ) lua_touserdata( L, lua_upvalueindex( 1 ) );
					return apply_closure<Ret, Args>( L, *fn );
				};
	
				if constexpr ( !std::is_trivially_destructible_v<Func> )
				{
					lua_createtable( L, 0, 1 );
					stack::push<cfunction_t>( L, [ ] ( lua_State* L )
					{
						std::destroy_at( ( Func* ) lua_touserdata( L, 1 ) );
						return 0;
					} );
					stack::set_field( L, -2, meta::gc );
					lua_setfield( L, -2, "__gc" );
					lua_setmetatable( L, -2 );
				}
			}
			// Function pointer:
			//
			else if constexpr ( std::is_void_v<C> )
			{
				upvalue_count = 1;
				lua_pushlightuserdata( L, ( void* ) func );
	
				wrapper = [ ] ( lua_State* L ) -> int
				{
					auto fn = ( Func ) lua_touserdata( L, lua_upvalueindex( 1 ) );
					return apply_closure<Ret, Args>( L, fn );
				};
			}
			// Member function:
			//
			else
			{
				return [ & ] <typename... Tx> ( std::type_identity<std::tuple<Tx...>> )
				{
					return push_closure( L, [ func ] ( C& owner, Tx&&... args ) -> Ret
					{
						return ( owner.*func )( std::forward<Tx>( args )... );
					} );
				}( std::type_identity<Args>{} );
			}
			lua_pushcclosure( L, wrapper, upvalue_count );
		}
		
		// Pushes a constant closure.
		//
		template<auto F>
		static void push_closure( lua_State* L, const_tag<F> )
		{
			using Func =   decltype( F );
			using Traits = detail::function_traits<Func>;
			using Args =   typename Traits::arguments;
			using Ret =    typename Traits::return_type;
			using C =      typename Traits::owner;
	
			// Static function:
			//
			if constexpr ( Traits::is_lambda || std::is_void_v<C> )
			{
				cfunction_t wrapper = [ ] ( lua_State* L ) -> int
				{
					auto fn = F;
					return apply_closure<Ret, Args>( L, fn );
				};
				lua_pushcclosure( L, wrapper, 0 );
			}
			// Member function:
			//
			else
			{
				return [ & ] <typename... Tx> ( std::type_identity<std::tuple<Tx...>> )
				{
					return stack::push( L, [] ( C& owner, Tx&&... args ) -> Ret
					{
						return ( owner.*F )( std::forward<Tx>( args )... );
					} );
				}( std::type_identity<Args>{} );
			}
		}
	};

	// Type traits for invocables.
	//
	template<auto F> requires detail::Invocable<decltype(F)>
	struct type_traits<detail::const_tag<F>>
	{
		inline static void push( lua_State* L, detail::const_tag<F> ) 
		{
			detail::push_closure( L, detail::const_tag<F>{} );
		}
	};
	template<typename F> requires ( !Reference<std::decay_t<F>> && detail::Invocable<std::decay_t<F>> && !std::is_same_v<std::decay_t<F>, cfunction_t> )
	struct type_traits<F>
	{
		inline static void push( lua_State* L, F&& func ) 
		{
			detail::push_closure( L, std::forward<F>( func ) );
		}
	};
};