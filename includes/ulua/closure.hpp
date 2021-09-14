#pragma once
#include "common.hpp"
#include "stack.hpp"
#include "reference.hpp"

namespace ulua
{
	struct state_view;

	namespace detail
	{
		// Stack get all wrapper with handling for state_view.
		//
		template<typename T>
		struct arg_referencer;
		template<typename... Tx>
		struct arg_referencer<std::tuple<Tx...>>
		{
			inline auto operator()( lua_State* L ) const
			{
				return stack::get_all<std::tuple<Tx...>>( L, 1 );
			}
		};
		template<typename... Tx>
		struct arg_referencer<std::tuple<state_view, Tx...>>
		{
			inline auto operator()( lua_State* L ) const
			{
				return std::tuple_cat( std::tuple{ L }, stack::get_all<std::tuple<Tx...>>( L, 1 ) );
			}
		};

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
					Ret result = std::apply( Func{}, arg_referencer<Args>{}( L ) );
					if constexpr ( detail::is_tuple_v<Ret> )
					{
						stack::push_all( L, std::move( result ) );
						return ( int ) std::tuple_size_v<Ret>;
					}
					else
					{
						stack::push( L, std::move( result ) );
						return 1;
					}
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
					Ret result = std::apply( *fn, arg_referencer<Args>{}( L ) );
					if constexpr ( detail::is_tuple_v<Ret> )
					{
						stack::push_all( L, std::move( result ) );
						return ( int ) std::tuple_size_v<Ret>;
					}
					else
					{
						stack::push( L, std::move( result ) );
						return 1;
					}
				};
	
				if constexpr ( !std::is_trivially_destructible_v<Func> )
				{
					lua_createtable( L, 0, 1 );
					lua_pushcfunction( L, [ ] ( lua_State* L )
					{
						std::destroy_at( ( Func* ) lua_touserdata( L, 1 ) );
					} );
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
					Ret result = std::apply( fn, arg_referencer<Args>{}( L ) );
					if constexpr ( detail::is_tuple_v<Ret> )
					{
						stack::push_all( L, std::move( result ) );
						return ( int ) std::tuple_size_v<Ret>;
					}
					else
					{
						stack::push( L, std::move( result ) );
						return 1;
					}
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
					Ret result = std::apply( F, arg_referencer<Args>{}( L ) );
					if constexpr ( detail::is_tuple_v<Ret> )
					{
						stack::push_all( L, std::move( result ) );
						return ( int ) std::tuple_size_v<Ret>;
					}
					else
					{
						stack::push( L, std::move( result ) );
						return 1;
					}
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