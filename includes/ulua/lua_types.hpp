#pragma once
#include <lua.hpp>
#include <type_traits>
#include <string_view>
#include <variant>
#include <tuple>
#include <optional>
#include "common.hpp"

namespace ulua
{	
	enum class value_type
	{
		nil =            LUA_TNIL,
		boolean =        LUA_TBOOLEAN,
		light_userdata = LUA_TLIGHTUSERDATA,
		number =         LUA_TNUMBER,
		string =         LUA_TSTRING,
		table =          LUA_TTABLE,
		function =       LUA_TFUNCTION,
		userdata =       LUA_TUSERDATA,
		thread =         LUA_TTHREAD,
	};
	inline const char* type_name( value_type type ) { return lua_typename( nullptr, ( int ) type ); }

	// Metatable fields.
	//
	enum class meta : uint8_t
	{
		metatable, newindex,  index, gc,
		tostring,  name,      len,   ipairs,
		pairs,     unm,       add,   sub,
		mul,       div,       idiv,  mod,
		pow,       concat,    eq,    lt,
		le
	};
	inline constexpr const char* metafield_name( meta field ) 
	{
		constexpr const char* arr[] = {
			"__metatable", "__newindex", "__index",     "__gc",
			"__tostring",  "__name",     "__len",       "__ipairs",
			"__pairs",     "__unm",      "__add",       "__sub",
			"__mul",       "__div",      "__idiv",      "__mod",
			"__pow",       "__concat",   "__eq",        "__lt",
			"__le"
		};
		return arr[ ( size_t ) field ];
	}

	// Primitive types.
	//
	struct nil_t { struct tag {}; constexpr explicit nil_t( tag ) {} };
	inline constexpr nil_t nil{ nil_t::tag{} };

	using cfunction_t = lua_CFunction;
	
	struct light_userdata
	{
		void* pointer;
		operator void*() const { return pointer; }
	};
	// Type meta traits.
	//
	template<typename T> struct type_traits;

	struct popable_tag_t {};
	struct emplacable_tag_t {};
	struct deferrable_tag_t {};
	template<typename T> concept Poppable = std::is_base_of_v<popable_tag_t, type_traits<T>>;
	template<typename T> concept Emplacable = std::is_base_of_v<emplacable_tag_t, type_traits<T>>;
	template<typename T> concept Deferrable = std::is_base_of_v<deferrable_tag_t, type_traits<T>>;

	// Primitive type traits.
	//
	template<typename T> struct type_traits<T&> : type_traits<T> {};
	template<typename T> struct type_traits<T&&> : type_traits<T> {};
	template<typename T> struct type_traits<const T> : type_traits<T> {};
	template<typename T, size_t N> struct type_traits<T[N]> : type_traits<T*> {};
	template<typename T, size_t N> struct type_traits<T(&)[N]> : type_traits<T*> {};
	template<typename T, size_t N> struct type_traits<T(*)[N]> : type_traits<T*> {};

	template<typename T> requires std::is_integral_v<T>
	struct type_traits<T>
	{
		inline static int push( lua_State* L, T value )
		{
			lua_pushinteger( L, value );
			return 1;
		}
		inline static bool check( lua_State* L, int& idx )
		{
			return lua_type( L, idx++ ) == ( int ) value_type::number;
		}
		inline static T get( lua_State* L, int& idx )
		{
			return ( T ) luaL_checkinteger( L, idx++ );
		}
	};
	template<typename T> requires std::is_floating_point_v<T>
	struct type_traits<T>
	{
		inline static int push( lua_State* L, T value )
		{
			lua_pushnumber( L, value );
			return 1;
		}
		inline static bool check( lua_State* L, int& idx )
		{
			return lua_type( L, idx++ ) == ( int ) value_type::number;
		}
		inline static T get( lua_State* L, int& idx )
		{
			return ( T ) luaL_checknumber( L, idx++ );
		}
	};
	template<>
	struct type_traits<std::string_view>
	{
		inline static int push( lua_State* L, std::string_view value )
		{
			lua_pushlstring( L, value.data(), value.length() );
			return 1;
		}
		inline static bool check( lua_State* L, int& idx )
		{
			return lua_type( L, idx++ ) == ( int ) value_type::string;
		}
		inline static std::string_view get( lua_State* L, int& idx )
		{
			size_t length;
			const char* data = luaL_checklstring( L, idx++, &length );
			return { data, data + length };
		}
	};
	template<>
	struct type_traits<const char*> : type_traits<std::string_view>
	{
		inline static const char* get( lua_State* L, int& idx ) { return luaL_checkstring( L, idx++ ); }
	};
	template<>
	struct type_traits<std::string> : type_traits<std::string_view>
	{
		inline static std::string get( lua_State* L, int& idx ) { return std::string{ type_traits<std::string_view>::get( L, idx ) }; }
	};
	template<>
	struct type_traits<nil_t>
	{
		inline static int push( lua_State* L, nil_t )
		{
			lua_pushnil( L );
			return 1;
		}
		inline static bool check( lua_State* L, int& idx )
		{
			return lua_type( L, idx++ ) <= ( int ) value_type::nil;
		}
		inline static nil_t get( lua_State*, int& idx )
		{
			idx++;
			return nil;
		}
	};
	template<>
	struct type_traits<bool>
	{
		inline static int push( lua_State* L, bool value )
		{
			lua_pushboolean( L, value );
			return 1;
		}
		inline static bool check( lua_State* L, int& idx )
		{
			return lua_type( L, idx++ ) == ( int ) value_type::boolean;
		}
		inline static bool get( lua_State* L, int& idx )
		{
			return lua_toboolean( L, idx++ );
		}
	};
	template<>
	struct type_traits<cfunction_t>
	{
		inline static int push( lua_State* L, cfunction_t value )
		{
			lua_pushcfunction( L, value );
			return 1;
		}
		inline static bool check( lua_State* L, int& idx )
		{
			return lua_type( L, idx++ ) == ( int ) value_type::function;
		}
		inline static cfunction_t get( lua_State* L, int& idx )
		{
			return lua_tocfunction( L, idx++ );
		}
	};
	template<>
	struct type_traits<light_userdata>
	{
		inline static int push( lua_State* L, light_userdata value )
		{
			lua_pushlightuserdata( L, value );
			return 1;
		}
		inline static bool check( lua_State* L, int& idx )
		{
			return lua_type( L, idx++ ) == ( int ) value_type::light_userdata;
		}
		inline static light_userdata get( lua_State* L, int& idx )
		{
			return { lua_touserdata( L, idx++ ) };
		}
	};
	namespace detail
	{
		template<typename... Tx>
		struct deferred_variant
		{
			lua_State* L;
			int idx;
			size_t type_index;

			template<typename... Tv>
			inline operator std::variant<Tv...>() const
			{
				return detail::visit_index<sizeof...( Tx )>( type_index, [ & ] <size_t I> ( const_tag<I> ) -> std::variant<Tv...>
				{
					int i = idx;
					return type_traits<std::tuple_element_t<I, std::tuple<Tx...>>>::get( L, i );
				} );
			}
		};
		template<typename T>
		inline decltype( auto ) get_deferred_if( lua_State* L, int& idx )
		{
			if constexpr ( Deferrable<T> )
				return type_traits<T>::get_deferred( L, idx );
			else
				return type_traits<T>::get( L, idx );
		}
	};
	template<typename... Tx>
	struct type_traits<std::variant<Tx...>> : deferrable_tag_t
	{
		template<typename Var>
		inline static int push( lua_State* L, Var&& value )
		{
			size_t idx = value.index();
			if ( idx == std::variant_npos ) [[unlikely]]
				return type_traits<nil_t>::push( L, nil );
			return detail::visit_variant( std::forward<Var>( value ), [ & ] <typename T> ( T&& v )
			{
				return type_traits<T>::push( L, std::forward<T>( v ) );
			} );
		}
		inline static bool check( lua_State* L, int& idx )
		{
			bool valid = false;
			detail::enum_indices<sizeof...( Tx )>( [ & ] <size_t N> ( detail::const_tag<N> )
			{
				using T = std::variant_alternative_t<N, std::variant<Tx...>>;
				if ( valid ) return;

				int i = idx;
				if ( type_traits<T>::check( L, i ) )
				{
					idx = i;
					valid = true;
				}
			} );
			return valid;
		}
		inline static detail::deferred_variant<Tx...> get_deferred( lua_State* L, int& idx )
		{
			detail::deferred_variant<Tx...> result = { L, idx, std::variant_npos };
			detail::enum_indices<sizeof...( Tx )>( [ & ] <size_t N> ( detail::const_tag<N> )
			{
				using T = std::variant_alternative_t<N, std::variant<Tx...>>;
				if ( result.type_index != std::variant_npos ) return;

				int i = idx;
				if ( type_traits<T>::check( L, i ) )
				{
					idx = i;
					result.type_index = N;
				}
			} );
			if ( result.type_index == std::variant_npos )
				detail::type_error( L, idx, "variant<...>" /*TODO: Namer*/ );
			return result;
		}
		using variant_result_t = std::variant<decltype( type_traits<Tx>::get( std::declval<lua_State*>(), std::declval<int&>() ) )...>;
		inline static variant_result_t get( lua_State* L, int& idx ) { return get_deferred( L, idx ); }
	};
	template<typename T>
	struct type_traits<std::optional<T>> : deferrable_tag_t
	{
		template<typename Opt>
		inline static int push( lua_State* L, Opt&& value )
		{
			if ( !value ) return type_traits<nil_t>::push( L, nil );
			else          return type_traits<typename Opt::value_type>::push( L, std::forward<Opt>( value ).value() );
		}
		inline static bool check( lua_State* L, int& idx )
		{
			int i = idx;
			if ( type_traits<nil_t>::check( L, idx ) )
				return true;
			return type_traits<T>::check( L, ( idx = i ) );
		}
		inline static auto get_deferred( lua_State* L, int& idx )
		{
			using R = std::optional<decltype( detail::get_deferred_if<T>( L, idx ) )>;

			int i = idx;
			if ( type_traits<nil_t>::check( L, idx ) )
				return R{ std::nullopt };
			else
				return R{ detail::get_deferred_if<T>( L, ( idx = i ) ) };
		}
		inline static auto get( lua_State* L, int& idx )
		{
			using R = std::optional<decltype( type_traits<T>::get( L, idx ) )>;
			
			int i = idx;
			if ( type_traits<nil_t>::check( L, idx ) )
				return R{ std::nullopt };
			else
				return R{ type_traits<T>::get( L, ( idx = i ) ) };
		}
	};
	template<typename... Tx>
	struct type_traits<std::tuple<Tx...>> : deferrable_tag_t
	{
		template<typename Tup>
		inline static int push( lua_State* L, Tup&& value )
		{
			int res = 0;
			detail::enum_tuple( std::forward<Tup>( value ), [ & ] <typename T> ( T&& field )
			{
				res += type_traits<T>::push( L, std::forward<T>( field ) );
			} );
			return res;
		}
		inline static bool check( lua_State* L, int& idx )
		{
			bool valid = true;
			detail::enum_indices<sizeof...( Tx )>( [ & ] <size_t N> ( detail::const_tag<N> )
			{
				valid = valid && type_traits<std::tuple_element_t<N, std::tuple<Tx...>>>::check( L, idx );
			} );
			return valid;
		}
		inline static auto get_deferred( lua_State* L, int& idx )
		{
			return detail::ordered_forward_as_tuple{ detail::get_deferred_if<Tx>( L, idx )... }.unwrap();
		}
		inline static auto get( lua_State* L, int& idx )
		{
			return detail::ordered_forward_as_tuple{ type_traits<Tx>::get( L, idx )... }.unwrap();
		}
	};
	template<typename T1, typename T2>
	struct type_traits<std::pair<T1, T2>> : deferrable_tag_t
	{
		template<typename Pair>
		inline static int push( lua_State* L, Pair&& value )
		{
			int res = 0;
			detail::enum_tuple( std::forward<Pair>( value ), [ & ] <typename T> ( T && field )
			{
				res += type_traits<T>::push( L, std::forward<T>( field ) );
			} );
			return res;
		}
		inline static bool check( lua_State* L, int& idx )
		{
			return type_traits<T1>::check( L, idx ) && type_traits<T2>::check( L, idx );
		}
		inline static auto get_deferred( lua_State* L, int& idx )
		{
			return detail::ordered_forward_as_pair{ detail::get_deferred_if<T1>( L, idx ), detail::get_deferred_if<T2>( L, idx ) }.unwrap();
		}
		inline static auto get( lua_State* L, int& idx )
		{
			return detail::ordered_forward_as_pair{ type_traits<T1>::get( L, idx ), type_traits<T2>::get( L, idx ) }.unwrap();
		}
	};

	// Pseudo type traits that simply return the active state.
	//
	template<>
	struct type_traits<lua_State*>
	{
		inline static bool check( lua_State*, int& ) { return true; }
		inline static auto get( lua_State* L, int& ) { return L; }
	};
	struct state_view;
	template<> struct type_traits<state_view> : type_traits<lua_State*> {};
};