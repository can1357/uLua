#pragma once
#include "common.hpp"
#include "stack.hpp"
#include "reference.hpp"

namespace ulua
{
	struct push_count { int n; };

	namespace detail
	{
		// Applies a function an pushes the result.
		//
		template<typename Ret, typename Args, typename F>
		static int apply_closure( lua_State* L, F& func )
		{
			return std::apply( [ & ] <typename... Tx> ( Tx&&... args ) -> int 
			{
				if constexpr ( std::is_void_v<Ret> )
				{
					func( std::forward<Tx>( args )... );
					return 0;
				}
				else
				{
					Ret result = func( std::forward<Tx>( args )... );
					if constexpr ( std::is_same_v<push_count, std::decay_t<Ret>> )
						return result.n;
					else
						return stack::push( L, std::forward<Ret>( result ) );
				}
			}, stack::get<Args>( L, 1 ) );
		}

		// Pushes a runtime closure.
		//
		template<typename F>
		static int push_closure( lua_State* L, F&& func )
		{
			using Func =   std::decay_t<F>;
			using Traits = detail::function_traits<Func>;
			using Args =   typename Traits::arguments;
			using Ret =    typename Traits::return_type;
			using C =      typename Traits::owner;
			static_assert( !Traits::is_vararg, "C varag functions are not allowed." );
	
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
				stack::emplace_userdata<Func>( L, std::forward<F>( func ) );
	
				wrapper = [ ] ( lua_State* L ) -> int
				{
					int uvi = lua_upvalueindex( 1 );
					auto* fn = ( Func* ) type_traits<userdata_value>::get( L, uvi ).pointer;
					return apply_closure<Ret, Args>( L, *fn );
				};
	
				if constexpr ( !std::is_trivially_destructible_v<Func> )
				{
					stack::create_table( L, reserve_records{ 1 } );
					stack::push<cfunction_t>( L, [ ] ( lua_State* L )
					{
						int uvi = 1;
						auto* fn = ( Func* ) type_traits<userdata_value>::get( L, uvi ).pointer;
						std::destroy_at( fn );
						return 0;
					} );
					stack::set_field( L, -2, meta::gc );
					stack::set_metatable( L, -2 );
				}
			}
			// Function pointer:
			//
			else if constexpr ( std::is_void_v<C> )
			{
				upvalue_count = 1;
				stack::push( L, light_userdata{ +func } );
	
				wrapper = [ ] ( lua_State* L ) -> int
				{
					int uvi = lua_upvalueindex( 1 );
					auto fn = ( decltype( +func ) ) type_traits<light_userdata>::get( L, uvi ).pointer;
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
			stack::push_closure( L, wrapper, upvalue_count );
			return 1;
		}
		
		// Pushes a constant closure.
		//
		template<auto F>
		static int push_closure( lua_State* L, const_tag<F> )
		{
			using Func =   decltype( F );
			using Traits = detail::function_traits<Func>;
			using Args =   typename Traits::arguments;
			using Ret =    typename Traits::return_type;
			using C =      typename Traits::owner;
			static_assert( !Traits::is_vararg, "C varag functions are not allowed." );

			// Static function:
			//
			if constexpr ( Traits::is_lambda || std::is_void_v<C> )
			{
				stack::push_closure( L, [ ] ( lua_State* L ) -> int
				{
					auto fn = F;
					return apply_closure<Ret, Args>( L, fn );
				} );
				return 1;
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
	struct type_traits<const_tag<F>>
	{
		inline static int push( lua_State* L, const_tag<F> ) 
		{
			return detail::push_closure( L, const_tag<F>{} );
		}
	};
	template<typename F> requires ( !Reference<std::decay_t<F>> && detail::Invocable<std::decay_t<F>> && !std::is_same_v<std::decay_t<F>, cfunction_t> )
	struct type_traits<F>
	{
		template<typename V>
		inline static int push( lua_State* L, V&& func ) 
		{
			return detail::push_closure<V>( L, std::forward<V>( func ) );
		}
	};

	// Overload helper.
	//
	template<typename... Tx>
	struct overload : private Tx...
	{
		using arguments =   std::variant<detail::popped_vtype_t<typename detail::function_traits<Tx>::arguments>...>;
		
		inline constexpr overload() requires ( std::is_default_constructible_v<Tx> && ... ) = default;
		inline constexpr overload( Tx&&... fn ) : Tx( std::forward<Tx>( fn ) )... {}

		inline push_count operator()( lua_State* L, arguments arg ) const
		{ 
			return detail::visit_index<sizeof...( Tx )>( arg.index(), [ & ] <size_t I> ( ulua::const_tag<I> )
			{
				return push_count{ stack::push( L, std::apply( ( const std::tuple_element_t<I, std::tuple<Tx...>>& ) * this, std::move( std::get<I>( arg ) ) ) ) };
			} );
		}
	};
	template<typename... Tx> overload( Tx&&... )->overload<Tx...>;
};