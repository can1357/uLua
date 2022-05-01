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
#if ULUA_JIT
		cdata =          LUA_TCDATA
#endif
	};
	inline const char* type_name( value_type type ) { return lua_typename( nullptr, ( int ) type ); }

	// Define stack type-checks here as stack header depends on this one.
	//
	namespace stack
	{
		// Gets the type of the value in the stack slot.
		//
		ULUA_INLINE inline value_type type( lua_State* L, int i )
		{
			return ( value_type ) lua_type( L, i );
		}

		// Checks the type of the value in the stack against a known type.
		//
		template<value_type T>
		ULUA_INLINE inline bool type_check( lua_State* L, int i )
		{
#if !ULUA_ACCEL
			if constexpr ( T == value_type::nil )
				return lua_type( L, idx++ ) <= ( int ) value_type::nil;
			else
				return type( L, i ) == T;
#else
			switch ( T )
			{
				case value_type::nil:            return tvisnil( accel::ref( L, i ) );
				case value_type::boolean:        return tvisbool( accel::ref( L, i ) );
				case value_type::light_userdata: return tvislightud( accel::ref( L, i ) );
				case value_type::number:         return tvisnumber( accel::ref( L, i ) );
				case value_type::string:         return tvisstr( accel::ref( L, i ) );
				case value_type::table:          return tvistab( accel::ref( L, i ) );
				case value_type::function:       return tvisfunc( accel::ref( L, i ) );
				case value_type::userdata:       return tvisudata( accel::ref( L, i ) );
				case value_type::thread:         return tvisthread( accel::ref( L, i ) );
#if ULUA_JIT
				case value_type::cdata:          return tviscdata( accel::ref( L, i ) );
#endif
				default: break;
			}
#endif
		}
	};

	// Metatable fields.
	//
	enum class meta : uint8_t
	{
		metatable, newindex,  index, gc,
		tostring,  name,      len,   ipairs,
		pairs,     unm,       add,   sub,
		mul,       div,       idiv,  mod,
		pow,       concat,    eq,    lt,
		le,        call,      mode
	};
	inline constexpr const char* metafield_name( meta field ) 
	{
		constexpr const char* arr[] = {
			"__metatable", "__newindex", "__index",     "__gc",
			"__tostring",  "__name",     "__len",       "__ipairs",
			"__pairs",     "__unm",      "__add",       "__sub",
			"__mul",       "__div",      "__idiv",      "__mod",
			"__pow",       "__concat",   "__eq",        "__lt",
			"__le",        "__call",     "__mode"
		};
		return arr[ ( size_t ) field ];
	}

	// Primitive types.
	//
	struct nil_t
	{
		struct tag {};
		constexpr explicit nil_t( tag ) {}
		constexpr bool operator==( nil_t ) const noexcept { return true; }
		constexpr bool operator!=( nil_t ) const noexcept { return false; }
	};
	inline constexpr nil_t nil{ nil_t::tag{} };

	using cfunction_t = lua_CFunction;
	
	struct light_userdata
	{
		void* pointer;
		operator void* ( ) const { return pointer; }
	};
	struct userdata_value
	{
		void* pointer;
		operator void* () const { return pointer; }
	};

	// Type traits declaration.
	//
	template<typename T> struct type_traits;
	template<typename T> struct type_traits<T&> : type_traits<T> {};
	template<typename T> struct type_traits<T&&> : type_traits<T> {};
	template<typename T> struct type_traits<const T> : type_traits<T> {};
	template<typename T, size_t N> struct type_traits<T[ N ]> : type_traits<T*> {};
	template<typename T, size_t N> struct type_traits<T( & )[ N ]> : type_traits<T*> {};
	template<typename T, size_t N> struct type_traits<T( * )[ N ]> : type_traits<T*> {};
	template<typename R, typename... A> struct type_traits<R(A...)> : type_traits<R(*)( A... )> {};
	template<typename R, typename... A> struct type_traits<R(A..., ...)> : type_traits<R(*)( A..., ... )> {};

	// Type meta traits.
	//
	struct popable_tag_t {};
	struct emplacable_tag_t {};
	template<typename T> concept Poppable = std::is_base_of_v<popable_tag_t, type_traits<T>>;
	template<typename T> concept Emplacable = std::is_base_of_v<emplacable_tag_t, type_traits<T>>;

	// Primitive type traits.
	//
	template<typename T> requires std::is_integral_v<T>
	struct type_traits<T>
	{
		ULUA_INLINE static int push( lua_State* L, T value )
		{
#if ULUA_ACCEL
			setintptrV( L->top, value );
			incr_top( L );
#else
			lua_pushinteger( L, value );
#endif
			return 1;
		}
		ULUA_INLINE static bool check( lua_State* L, int& idx )
		{
			return stack::type_check<value_type::number>( L, idx++ );
		}
		ULUA_INLINE static T get( lua_State* L, int& idx )
		{
#if ULUA_ACCEL
			auto* tv = accel::ref( L, idx++ );
			if ( tvisint( tv ) )
				return ( T ) intV( tv );
			else if ( tvisnum( tv ) )
				return ( T ) numV( tv );
			else
				type_error( L, idx - 1, "integer" );
#else
			return ( T ) luaL_checkinteger( L, idx++ );
#endif
		}
	};
	template<typename T> requires std::is_enum_v<T>
	struct type_traits<T>
	{
		using U = std::underlying_type_t<T>;
		ULUA_INLINE static int push( lua_State* L, T value ) { return type_traits<U>::push( L, ( U ) value ); }
		ULUA_INLINE static bool check( lua_State* L, int& idx ) { return type_traits<U>::check( L, idx ); }
		ULUA_INLINE static T get( lua_State* L, int& idx ) { return ( T ) type_traits<U>::get( L, idx ); }
	};
	template<typename T> requires std::is_floating_point_v<T>
	struct type_traits<T>
	{
		ULUA_INLINE static int push( lua_State* L, T value )
		{
#if ULUA_ACCEL
			setnumV( L->top, value );
			if ( value != value ) [[unlikely]]
				setnanV( L->top );
			incr_top( L );
#else
			lua_pushnumber( L, value );
#endif
			return 1;
		}
		ULUA_INLINE static bool check( lua_State* L, int& idx )
		{
			return stack::type_check<value_type::number>( L, idx++ );
		}
		ULUA_INLINE static T get( lua_State* L, int& idx )
		{
#if ULUA_ACCEL
			auto* tv = accel::ref( L, idx++ );
			if ( tvisint( tv ) )
				return ( T ) intV( tv );
			else if ( tvisnum( tv ) )
				return ( T ) numV( tv );
			else
				type_error( L, idx - 1, "number" );
#else
			return ( T ) luaL_checknumber( L, idx++ );
#endif
		}
	};
	template<>
	struct type_traits<std::string_view>
	{
		ULUA_INLINE static int push( lua_State* L, std::string_view value )
		{
#if ULUA_ACCEL
			setstrV( L, L->top, lj_str_new( L, value.data(), value.size() ) );
			incr_top( L );
#else
			lua_pushlstring( L, value.data(), value.length() );
#endif
			return 1;
		}
		ULUA_INLINE static bool check( lua_State* L, int& idx )
		{
			return stack::type_check<value_type::string>( L, idx++ );
		}
		ULUA_INLINE static std::string_view get( lua_State* L, int& idx )
		{
#if ULUA_ACCEL
			auto* tv = accel::ref( L, idx++ );
			if ( !tvisstr( tv ) )
				type_error( L, idx - 1, "string" );
			auto* s = strV( tv );
			return { strdata( s ), strdata( s ) + s->len };
#else
			size_t length;
			const char* data = luaL_checklstring( L, idx++, &length );
			return { data, data + length };
#endif
		}
	};
	template<>
	struct type_traits<const char*> : type_traits<std::string_view>
	{
		ULUA_INLINE static const char* get( lua_State* L, int& idx ) { return luaL_checkstring( L, idx++ ); }
	};
	template<>
	struct type_traits<std::string> : type_traits<std::string_view>
	{
		ULUA_INLINE static std::string get( lua_State* L, int& idx ) { return std::string{ type_traits<std::string_view>::get( L, idx ) }; }
	};
	template<>
	struct type_traits<nil_t>
	{
		ULUA_INLINE static int push( lua_State* L, nil_t )
		{
#if ULUA_ACCEL
			setnilV( L->top );
			incr_top( L );
#else
			lua_pushnil( L );
#endif
			return 1;
		}
		ULUA_INLINE static bool check( lua_State* L, int& idx )
		{
			return stack::type_check<value_type::nil>( L, idx++ );
		}
		ULUA_INLINE static nil_t get( lua_State*, int& idx )
		{
			idx++;
			return nil;
		}
	};
	template<>
	struct type_traits<bool>
	{
		ULUA_INLINE static int push( lua_State* L, bool value )
		{
#if ULUA_ACCEL
			setboolV( L->top, value );
			incr_top( L );
#else
			lua_pushboolean( L, value );
#endif
			return 1;
		}
		ULUA_INLINE static bool check( lua_State* L, int& idx )
		{
			return stack::type_check<value_type::boolean>( L, idx++ );
		}
		ULUA_INLINE static bool get( lua_State* L, int& idx )
		{
#if ULUA_ACCEL
			return tvistruecond( accel::ref( L, idx++ ) );
#else
			return lua_toboolean( L, idx++ );
#endif
		}
	};
	template<>
	struct type_traits<cfunction_t>
	{
		ULUA_INLINE static int push( lua_State* L, cfunction_t value )
		{
			lua_pushcfunction( L, value );
			return 1;
		}
		ULUA_INLINE static bool check( lua_State* L, int& idx )
		{
			return lua_tocfunction( L, idx++ );
		}
		ULUA_INLINE static cfunction_t get( lua_State* L, int& idx )
		{
			int i = idx;
			if ( auto f = lua_tocfunction( L, idx++ ) ) [[likely]]
				return f;
			type_error( L, i, "C function" );
		}
	};
	template<>
	struct type_traits<light_userdata>
	{
		ULUA_INLINE static int push( lua_State* L, light_userdata value )
		{
			lua_pushlightuserdata( L, value );
			return 1;
		}
		ULUA_INLINE static bool check( lua_State* L, int& idx )
		{
			return stack::type_check<value_type::light_userdata>( L, idx++ );
		}
		ULUA_INLINE static light_userdata get( lua_State* L, int& idx )
		{
#if ULUA_ACCEL
			auto* tv = accel::ref( L, idx++ );
			if ( tvislightud( tv ) ) [[likely]]
				return { lightudV( G( L ), tv ) };
#else
			return { lua_touserdata( L, idx++ ) };
#endif
			return { nullptr }; // Caller should handle nullptr instead for better error messages.
		}
	};
	template<>
	struct type_traits<userdata_value>
	{
		// No pusher.
		ULUA_INLINE static bool check( lua_State* L, int& idx )
		{
			return stack::type_check<value_type::userdata>( L, idx++ );
		}
		ULUA_INLINE static userdata_value get( lua_State* L, int& idx )
		{
#if ULUA_ACCEL
			auto* tv = accel::ref( L, idx++ );
			if ( tvisudata( tv ) ) [[likely]]
				return { uddata( udataV( tv ) ) };
#else
			return { lua_touserdata( L, idx++ ) };
#endif
			return { nullptr }; // Caller should handle nullptr instead for better error messages.
		}
	};
	namespace detail
	{
		template<typename T> using popped_type_t = decltype( type_traits<T>::get( std::declval<lua_State*>(), std::declval<int&>() ) );
		template<typename T> struct popped_vtype;
		template<template<typename...> typename Tr, typename... Tx> struct popped_vtype<Tr<Tx...>> { using type = Tr<popped_type_t<Tx>...>; };
		template<typename T> using popped_vtype_t = typename popped_vtype<T>::type;
	};
	template<typename... Tx>
	struct type_traits<std::variant<Tx...>>
	{
		template<typename Var>
		ULUA_INLINE static int push( lua_State* L, Var&& value )
		{
			size_t idx = value.index();
			if ( idx == std::variant_npos ) [[unlikely]]
				return type_traits<nil_t>::push( L, nil );
			return detail::visit_variant( std::forward<Var>( value ), [ & ] <typename T> ( T&& v )
			{
				return type_traits<T>::push( L, std::forward<T>( v ) );
			} );
		}
		ULUA_INLINE static bool check( lua_State* L, int& idx )
		{
			bool valid = false;
			detail::enum_indices<sizeof...( Tx )>( [ & ] <size_t N> ( const_tag<N> )
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
		ULUA_INLINE static std::variant<Tx...> get( lua_State* L, int& idx )
		{
			std::optional<std::variant<Tx...>> result = {};
			detail::enum_indices<sizeof...( Tx )>( [ & ] <size_t N> ( const_tag<N> )
			{
				using T = std::variant_alternative_t<N, std::variant<Tx...>>;
				if ( result.has_value() ) return;

				int i = idx;
				if ( type_traits<T>::check( L, i ) )
					result.emplace( std::in_place_index_t<N>{}, type_traits<T>::get( L, idx ) );
			} );
			if ( !result.has_value() )
			{
#if ULUA_CTTI
				type_error( L, idx, "variant<...>" );
#else
				const char* name = detail::ctti_namer<std::variant<Tx...>>{};
				type_error( L, idx, name + 5 /*skip std::*/ );
#endif
			}
			return std::move( result ).value();
		}
	};
	template<typename T>
	struct type_traits<std::optional<T>>
	{
		template<typename Opt>
		ULUA_INLINE static int push( lua_State* L, Opt&& value )
		{
			if ( !value ) return type_traits<nil_t>::push( L, nil );
			else          return type_traits<typename std::remove_cvref_t<Opt>::value_type>::push( L, std::forward<Opt>( value ).value() );
		}
		ULUA_INLINE static bool check( lua_State* L, int& idx )
		{
			int i = idx;
			if ( type_traits<nil_t>::check( L, idx ) )
				return true;
			return type_traits<T>::check( L, ( idx = i ) );
		}
		ULUA_INLINE static auto get( lua_State* L, int& idx )
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
	struct type_traits<std::tuple<Tx...>>
	{
		template<typename Tup>
		ULUA_INLINE static int push( lua_State* L, Tup&& value )
		{
			int res = 0;
			detail::find_tuple_if( std::forward<Tup>( value ), [ & ] <typename T> ( T&& field )
			{
				res += type_traits<T>::push( L, std::forward<T>( field ) );
				return false;
			} );
			return res;
		}
		ULUA_INLINE static bool check( lua_State* L, int& idx )
		{
			bool valid = true;
			detail::enum_indices<sizeof...( Tx )>( [ & ] <size_t N> ( const_tag<N> )
			{
				valid = valid && type_traits<std::tuple_element_t<N, std::tuple<Tx...>>>::check( L, idx );
			} );
			return valid;
		}
		ULUA_INLINE static auto get( lua_State* L, int& idx )
		{
			return detail::ordered_forward_as_tuple{ type_traits<Tx>::get( L, idx )... }.unwrap();
		}
	};
	template<typename T1, typename T2>
	struct type_traits<std::pair<T1, T2>>
	{
		template<typename Pair>
		ULUA_INLINE static int push( lua_State* L, Pair&& value )
		{
			int res = 0;
			res += type_traits<T1>::push( L, std::get<0>( std::forward<Pair>( value ) ) );
			res += type_traits<T2>::push( L, std::get<1>( std::forward<Pair>( value ) ) );
			return res;
		}
		ULUA_INLINE static bool check( lua_State* L, int& idx )
		{
			return type_traits<T1>::check( L, idx ) && type_traits<T2>::check( L, idx );
		}
		ULUA_INLINE static auto get( lua_State* L, int& idx )
		{
			return detail::ordered_forward_as_pair{ type_traits<T1>::get( L, idx ), type_traits<T2>::get( L, idx ) }.unwrap();
		}
	};
	template<>
	struct type_traits<std::nullopt_t>
	{
		ULUA_INLINE static int push( lua_State* L, std::nullopt_t ) { return type_traits<nil_t>::push( L, nil ); }
		ULUA_INLINE static bool check( lua_State* L, int& idx ) { return type_traits<nil_t>::check( L, idx ); }
		ULUA_INLINE static std::nullopt_t get( lua_State*, int& idx ) { idx++; return std::nullopt; }
	};
	template<>
	struct type_traits<std::nullptr_t>
	{
		ULUA_INLINE static int push( lua_State* L, std::nullptr_t ) { return type_traits<nil_t>::push( L, nil ); }
		ULUA_INLINE static bool check( lua_State* L, int& idx ) { return type_traits<nil_t>::check( L, idx ); }
		ULUA_INLINE static std::nullptr_t get( lua_State*, int& idx ) { idx++; return nullptr; }
	};

	// FFI types.
	//
#if ULUA_JIT
	struct cdata_value
	{
		void* pointer;
		CTypeID  type;

		template<typename T>
		inline T& as() const { return *( T* ) pointer; }
	};

	template<>
	struct type_traits<cdata_value>
	{
		ULUA_INLINE static int push( lua_State* L, cdata_value value )
		{
			if ( !value.pointer )
				setnilV( L->top );
			else
				setcdataV( L, L->top, std::prev( ( GCcdata* ) value.pointer ) );
			incr_top( L );
			return 1;
		}
		ULUA_INLINE static bool check( lua_State* L, int& idx )
		{
			return stack::type_check<value_type::cdata>( L, idx++ );
		}
		ULUA_INLINE static cdata_value get( lua_State* L, int& idx )
		{
			auto* tv = accel::ref( L, idx++ );
			if ( tviscdata( tv ) ) [[likely]]
			{
				auto * cd = cdataV( tv );
				return { cdataptr( cd ), cd->ctypeid };
			}
			return { nullptr, CTypeID( -1 ) };
		}
	};
#endif

	// Pseudo type traits that simply return the active state.
	//
	template<>
	struct type_traits<lua_State*>
	{
		inline static bool check( lua_State*, int& ) { return true; }
		inline static auto get( lua_State* L, int& ) { return L; }
	};
	struct state_view;
	struct caller_reference;
	struct this_environment;
	template<> struct type_traits<state_view> :       type_traits<lua_State*> {};
	template<> struct type_traits<caller_reference> : type_traits<lua_State*> {};
	template<> struct type_traits<this_environment> : type_traits<lua_State*> {};
};