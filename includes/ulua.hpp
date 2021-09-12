#pragma once

// Lua headers.
//
#include <lua.hpp>
extern "C" {
	#include <lj_obj.h>
};

#include <type_traits>
#include <string>
#include <string_view>
#include <span>
#include <tuple>
#include <optional>
#include <algorithm>
#include <utility>

#ifdef XSTD_ESTR
	#define __XSTD__ 1
#else
	#define __XSTD__ 0
#endif

#pragma pack(push, 1)
namespace ulua
{
	// Lua types.
	//
	using nil_t = std::nullptr_t;
	static constexpr nil_t nil = nullptr;
	using cfunction_t = lua_CFunction;
	
	struct light_userdata
	{
		void* pointer;
		operator void*() const { return pointer; }
	};

	struct thread_instance
	{
		lua_State* L;
		operator lua_State*() const { return L; }
	};
	
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

	// Error wrappers.
	//
	namespace detail
	{
		// TODO: add more...
		inline void error [[noreturn]] ( lua_State* L, const char* fmt )
		{
			// TODO: Debug, remove.
			printf( "<< error: %s >>\n", fmt );
			luaL_error( L, fmt );
			unreachable();
		}

		inline void type_error [[noreturn]] ( lua_State* L, int arg, const char* type )
		{
			// TODO: Debug, remove.
			printf( "<< type error (%d): %s >>\n", arg, type );
			luaL_typerror( L, arg, type );
			unreachable();
		}
	};

	// Primitive type traits.
	//
	template<typename T>
	struct type_traits;
	template<typename T> requires std::is_integral_v<T>
	struct type_traits<T>
	{
		inline static void push( lua_State* L, T value )
		{
			lua_pushinteger( L, value );
		}
		inline static bool check( lua_State* L, int idx )
		{
			return lua_isnumber( L, idx );
		}
		inline static T get( lua_State* L, int idx )
		{
			return ( T ) luaL_checkinteger( L, idx );
		}
	};
	template<typename T> requires std::is_floating_point_v<T>
	struct type_traits<T>
	{
		inline static void push( lua_State* L, T value )
		{
			lua_pushnumber( L, value );
		}
		inline static bool check( lua_State* L, int idx )
		{
			return lua_isnumber( L, idx );
		}
		inline static T get( lua_State* L, int idx )
		{
			return ( T ) luaL_checknumber( L, idx );
		}
	};
	template<>
	struct type_traits<std::string_view>
	{
		inline static void push( lua_State* L, std::string_view value )
		{
			lua_pushlstring( L, value.data(), value.length() );
		}
		inline static bool check( lua_State* L, int idx )
		{
			return lua_isstring( L, idx );
		}
		inline static std::string_view get( lua_State* L, int idx )
		{
			size_t length;
			const char* data = luaL_checklstring( L, idx, &length );
			return { data, data + length };
		}
	};
	template<>
	struct type_traits<const char*>
	{
		inline static void push( lua_State* L, const char* value )
		{
			lua_pushstring( L, value );
		}
		inline static bool is( lua_State* L, int idx )
		{
			return lua_isstring( L, idx );
		}
		inline static const char* get( lua_State* L, int idx )
		{
			return luaL_checkstring( L, idx );
		}
	};
	template<typename C>
	struct type_traits<std::basic_string<C>>
	{
		inline static void push( lua_State* L, const std::basic_string<C>& value )
		{
#if __XSTD__
			std::string result = xstd::utf_convert<char>( value );
#else
			std::string result = { value.begin(), value.end() };
#endif
			type_traits<std::string_view>::push( L, result );
		}
		inline static bool check( lua_State* L, int idx )
		{
			return lua_isstring( L, idx );
		}
		inline static std::basic_string<C> get( lua_State* L, int idx )
		{
			auto sv = type_traits<std::string_view>::get( L, idx );
#if __XSTD__
			return xstd::utf_convert<C>( sv );
#else
			return { sv.begin(), sv.end() };
#endif
		}
	};
	template<>
	struct type_traits<nil_t>
	{
		inline static void push( lua_State* L, nil_t )
		{
			lua_pushnil( L );
		}
		inline static bool check( lua_State* L, int idx )
		{
			return lua_isnil( L, idx );
		}
		inline static nil_t get( lua_State* L, int idx )
		{
			return nil;
		}
	};
	template<>
	struct type_traits<bool>
	{
		inline static void push( lua_State* L, bool value )
		{
			lua_pushboolean( L, value );
		}
		inline static bool check( lua_State* L, int idx )
		{
			return lua_isboolean( L, idx );
		}
		inline static bool get( lua_State* L, int idx )
		{
			return lua_toboolean( L, idx );
		}
	};
	template<>
	struct type_traits<cfunction_t>
	{
		inline static void push( lua_State* L, cfunction_t value )
		{
			lua_pushcfunction( L, value );
		}
		inline static bool check( lua_State* L, int idx )
		{
			return lua_iscfunction( L, idx );
		}
		inline static cfunction_t get( lua_State* L, int idx )
		{
			if ( !check( L, idx ) )
				detail::type_error( L, idx, "C Function" );
			return lua_tocfunction( L, idx );
		}
	};
	template<>
	struct type_traits<light_userdata>
	{
		inline static void push( lua_State* L, light_userdata value )
		{
			lua_pushlightuserdata( L, value );
		}
		inline static bool check( lua_State* L, int idx )
		{
			return lua_islightuserdata( L, idx );
		}
		inline static light_userdata get( lua_State* L, int idx )
		{
			if ( !check( L, idx ) )
				detail::type_error( L, idx, "Light userdata" );
			return { lua_touserdata( L, idx ) };
		}
	};
	template<typename T> struct type_traits<T&> : type_traits<T> {};
	template<typename T> struct type_traits<T&&> : type_traits<T> {};
	template<typename T> struct type_traits<const T> : type_traits<T> {};
	template<typename T, size_t N> struct type_traits<T[N]> : type_traits<T*> {};
	template<typename T, size_t N> struct type_traits<T(&)[N]> : type_traits<T*> {};
	template<typename T, size_t N> struct type_traits<T(*)[N]> : type_traits<T*> {};

	// Reference tag.
	//
	struct reference_tag_t {};
	template<typename T>
	concept Reference = std::is_base_of_v<reference_tag_t, std::decay_t<T>>;

	// Typed reference tag.
	// -> must have static bool check( const stack_reference& ref );
	//
	struct typed_reference_tag_t {};
	template<typename T>
	concept TypedReference = std::is_base_of_v<typed_reference_tag_t, std::decay_t<T>>;

	// Stack helpers.
	//
	namespace stack
	{
		using slot = int;
		struct top_t { inline operator slot() const { return -1; } };

		inline slot top( lua_State* L ) { return ( slot ) ( L->top - L->base ); }
		inline void set_top( lua_State* L, slot i ) { lua_settop( L, i ); }
		inline void pop_n( lua_State* L, slot i ) { L->top -= i; }
		inline value_type type( lua_State* L, slot i ) { return ( value_type ) lua_type( L, i ); }

		inline constexpr bool is_relative( slot i ) { return i < 0 && i > LUA_REGISTRYINDEX; }
		inline constexpr bool is_global( slot i ) { return i < LUA_REGISTRYINDEX; }
		inline constexpr bool is_absolute( slot i ) { return i > 0; }

		inline slot abs( lua_State* L, slot i ) { return is_relative( i ) ? top( L ) + 1 + i : i; }
		inline slot rel( lua_State* L, slot i ) { return is_relative( i ) ? i : i - ( top( L ) + 1 ); }
		inline void remove( lua_State* L, slot i, size_t n = 1 ) 
		{ 
			while( n )
				lua_remove( L, i + --n );
		}
	};

	// Reference type.
	//
	struct weak_t {};
	struct stack_reference : reference_tag_t
	{
		static constexpr bool is_direct = true;

		lua_State* L = nullptr;
		int index = 0;
		bool valid_flag = false;
		bool ownership_flag = false;

		// Top helper.
		//
		inline static stack_reference top( lua_State* L, int i = 1 ) { return { L, stack::top( L ) + 1 - i }; }

		// Construction, copy & move.
		//
		inline constexpr stack_reference() {}
		inline stack_reference( lua_State* L, int index ) : L( L ), index( stack::abs( L, index ) ), valid_flag( true ), ownership_flag( true ) {}
		inline stack_reference( lua_State* L, int index, weak_t ) : L( L ), index( stack::abs( L, index ) ), valid_flag( true ), ownership_flag( false ) {}

		inline constexpr stack_reference( stack_reference&& o ) noexcept { swap( o ); }
		inline constexpr stack_reference& operator=( stack_reference&& o ) noexcept { swap( o ); return *this; }
		stack_reference( const stack_reference& ) = delete;
		stack_reference& operator=( const stack_reference& ) = delete;

		inline constexpr void swap( stack_reference& other ) noexcept
		{
			std::swap( L, other.L );
			std::swap( index, other.index );
			std::swap( valid_flag, other.valid_flag );
			std::swap( ownership_flag, other.ownership_flag );
		}

		// Generic reference functions.
		//
		inline lua_State* state() const { return L; }
		inline int slot() const { return index; }
		inline void push() const { lua_pushvalue( state(), slot() ); }
		inline bool valid() const { return valid_flag; }
		inline void release() { valid_flag = false; }
		inline void reset()
		{
			if ( std::exchange( valid_flag, false ) && std::exchange( ownership_flag, false ) && stack::is_absolute( index ) )
				lua_remove( L, index );
		}
		inline ~stack_reference() { reset(); }
	};
	struct registry_reference : reference_tag_t
	{
		static constexpr bool is_direct = false;

		lua_State* L = nullptr;
		int key = LUA_REFNIL;
		bool valid_flag = false;

		// Construction, copy & move.
		//
		inline constexpr registry_reference() {}
		inline constexpr registry_reference( lua_State* L, int key ) : L( L ), key( key ), valid_flag( true ) {}
		inline registry_reference( lua_State* L, stack::top_t ) : registry_reference( L, luaL_ref( L, LUA_REGISTRYINDEX ) ) {}

		inline constexpr registry_reference( registry_reference&& o ) noexcept { swap( o ); }
		inline constexpr registry_reference& operator=( registry_reference&& o ) noexcept { swap( o ); return *this; }
		template<Reference Ref> inline registry_reference( const Ref& o ) { assign( o ); }
		template<Reference Ref> inline registry_reference& operator=( const Ref& o ) { assign( o ); return *this; }
		inline constexpr void swap( registry_reference& other ) noexcept
		{
			std::swap( L, other.L );
			std::swap( key, other.key );
			std::swap( valid_flag, other.valid_flag );
		}
		template<typename Ref>
		inline void assign( const Ref& o )
		{
			reset();
			if ( o.valid() )
			{
				o.push();
				L = o.L;
				key = luaL_ref( L, LUA_REGISTRYINDEX );
				valid_flag = true;
			}
		}

		// Decay to stack reference.
		//
		inline operator stack_reference() const
		{
			if ( !valid() )
				return {};
			push();
			return { L, stack::top_t{} };
		}

		// Generic reference functions.
		//
		inline lua_State* state() const { return L; }
		inline int registry_index() const { return key; }
		inline void push() const { lua_rawgeti( L, LUA_REGISTRYINDEX, key ); }
		inline bool valid() const { return valid_flag; }
		inline void reset()
		{
			if ( std::exchange( valid_flag, false ) )
				luaL_unref( L, LUA_REGISTRYINDEX, key );
		}
		inline ~registry_reference() { reset(); }
	};

	// Declare type traits for reference types.
	//
	template<Reference R> requires ( !TypedReference<R> )
	struct type_traits<R>
	{
		inline static void push( lua_State* L, const R& ref ) { ref.push(); }
		inline static bool check( lua_State* L, int idx ) { return true; }
		//inline static R get( lua_State* L, int idx ) { R tmp{}; tmp.assign( stack_reference{ L, idx, weak_t{} } ); return tmp; }
		inline static R pop( lua_State* L ) { return R{ L, stack::top_t{} }; }
	};
	template<TypedReference R>
	struct type_traits<R>
	{
		inline static void push( lua_State* L, const R& ref ) { ref.push(); }
		inline static bool check( lua_State* L, int idx ) { return std::decay_t<R>::check( stack_reference{ L, idx, weak_t{} } ); }
		//inline static R get( lua_State* L, int idx ) 
		//{ 
		//	if ( !check( L, idx ) )
		//		detail::type_error( L, idx, "Lua object" );
		//	R tmp{}; tmp.assign( stack_reference{ L, idx, weak_t{} } ); return tmp; 
		//}
		inline static R pop( lua_State* L )
		{
			if ( !check( L, stack::top_t{} ) )
				detail::type_error( L, stack::top_t{}, "Lua object" );
			return R{ L, stack::top_t{} };
		}
	};

	// Implement stack::push/pop/get/check.
	//
	namespace stack
	{
		template<typename T> concept Poppable = requires { &type_traits<T>::pop; };

		inline void push( lua_State* L ) {}
		template<typename T, typename... Tx>
		inline void push( lua_State* L, T&& value, Tx&&... rest )
		{
			type_traits<T>::push( L, std::forward<T>( value ) );
			push( L, std::forward<Tx>( rest )... );
		}
		template<typename T>
		inline auto pop( lua_State* L )
		{
			if constexpr ( Poppable<T> )
			{
				return type_traits<T>::pop( L );
			}
			else
			{
				auto result = type_traits<T>::get( L, top_t{} );
				pop_n( L, 1 );
				return result;
			}
		}
		template<typename... Tx> requires( sizeof...( Tx ) > 1 )
		inline auto pop( lua_State* L )
		{
			return std::tuple{ pop<Tx>( L )... };
		}
		template<typename T>
		inline auto clone( lua_State* L )
		{
			lua_pushvalue( L, top_t{} );
			return pop<T>( L );
		}
		template<typename T>
		inline bool check( lua_State* L, slot i )
		{
			return type_traits<T>::check( L, i );
		}
		template<typename T>
		inline auto get( lua_State* L, slot i )
		{
			return type_traits<T>::get( L, i );
		}
		template<typename T>
		inline bool check( const stack_reference& ref ) 
		{ 
			return stack::check<T>( ref.state(), ref.slot() ); 
		}
		template<typename T> 
		inline auto get( const stack_reference& ref ) 
		{ 
			return stack::get<T>( ref.state(), ref.slot() ); 
		}
		template<typename T> 
		inline bool check( const registry_reference& ref ) 
		{ 
			ref.push();
			bool res = stack::check<T>( ref.state(), top_t{} );
			stack::pop_n( ref.state(), 1 );
			return res;
		}
		template<typename T> 
		inline auto get( const registry_reference& ref ) 
		{
			ref.push();
			auto res = stack::get<T>( ref.state(), top_t{} );
			stack::pop_n( ref.state(), 1 );
			return res;
		}
	};

	// Table proxy.
	//
	struct raw_t {};
	namespace detail
	{
		// PUSH(Table[K]).
		//
		inline void get_table( const stack_reference& ref, const char* field )
		{
			lua_getfield( ref.state(), ref.slot(), field );
		}
		inline void get_table( const stack_reference& ref, int n )
		{
			stack::push( ref.state(), n );
			lua_gettable( ref.state(), ref.slot() );
		}
		inline void get_table( const stack_reference& ref, const char* field, raw_t )
		{
			stack::push( ref.state(), field );
			lua_rawget( ref.state(), ref.slot() );
		}
		inline void get_table( const stack_reference& ref, int n, raw_t )
		{
			lua_rawgeti( ref.state(), ref.slot(), n );
		}

		// Table[K] = POP().
		//
		inline void put_table( const stack_reference& ref, const char* field )
		{
			lua_setfield( ref.state(), ref.slot(), field );
		}
		inline void put_table( const stack_reference& ref, int n )
		{
			stack::push( ref.state(), n );
			lua_settable( ref.state(), ref.slot() );
		}
		inline void put_table( const stack_reference& ref, const char* field, raw_t )
		{
			stack::push( ref.state(), field );
			lua_rawset( ref.state(), ref.slot() );
		}
		inline void put_table( const stack_reference& ref, int n, raw_t )
		{
			lua_rawseti( ref.state(), ref.slot(), n );
		}
		inline std::string to_string( const stack_reference& ref )
		{
			value_type type = stack::type( ref.state(), ref.slot() );
			switch ( type )
			{
				case value_type::nil:            return "nil";
				case value_type::boolean:        return type_traits<bool>::get( ref.state(), ref.slot() ) ? "true" : "false";
				case value_type::light_userdata: return "light-userdata";
				case value_type::number:
				case value_type::string:         return type_traits<std::string>::get( ref.state(), ref.slot() );
				case value_type::function:       return "function";
				case value_type::thread:         return "thread";
				default:
				{
					//if ( call_meta( stack, value, "__tostring" ) )
					//{
					//	if ( stack.is<const char*>( stack::top_t{} ) )
					//		luaL_error( stack.state(), "'__tostring' must return a string" ); // TODO: Abstract
					//}
					//else if ( get_meta( stack, value, "__name" ) )
					//{
					//	if ( stack.is<const char*>( stack::top_t{} ) )
					//		luaL_error( stack.state(), "'__name' must contain a string" ); // TODO: Abstract
					//}
					//else
					
					return type == value_type::table ? "<table>" : "<userdata>";
				}
			}
		}

		// Is tuple or pair.
		//
		template<typename T> struct is_tuple { static constexpr bool value = false; };
		template<typename... Tx> struct is_tuple<std::tuple<Tx...>> { static constexpr bool value = true; };
		template<typename T1, typename T2> struct is_tuple<std::pair<T1, T2>> { static constexpr bool value = true; };
		template<typename T>
		static constexpr bool is_tuple_v = is_tuple<T>::value;
	};

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
		template<typename T> auto as( size_t i = 0 ) const 
		{ 
			if constexpr ( std::is_same_v<T, nil_t> )
				return T{};
			if ( i >= size() )
				detail::error( L, "Return count mismatch." );
			return stack::get<T>( L, first + i );
		}
		inline std::string to_string( size_t i ) const { return detail::to_string( get_ref( i ) ); }
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
				return [ & ] <template<typename...> typename Tup, typename... Tx> ( std::type_identity<Tup<Tx...>> ) -> Tup<Tx...>
				{
					if ( size() < sizeof...( Tx ) )
						detail::error( L, "Return count mismatch." );
					size_t it = 0;
					return Tup{ as<Tx>( it++ )... };
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

	// Table proxy.
	//
	template<typename Key, bool Raw>
	struct table_proxy
	{
		stack_reference table = {};
		Key key = {};
		explicit inline table_proxy( stack_reference table, Key key ) : table( std::move( table ) ), key( std::move( key ) ) {}

		// Ref getter.
		//
		inline void push() const
		{
			if constexpr ( Raw ) detail::get_table( table, key, raw_t{} );
			else                 detail::get_table( table, key );
		}
		inline stack_reference get_ref() const
		{
			push();
			return stack_reference{ table.state(), stack::top_t{} };
		}

		// Generic casts.
		//
		template<typename T> inline auto as() const { push(); return stack::pop<T>( table.state() ); }
		template<typename T> inline bool is() const { return stack::check<T>( get_ref() ); }
		template<typename T> inline operator T() const { push(); return stack::pop<T>( table.state() ); }
		inline std::string to_string() const { return detail::to_string( get_ref() ); }

		// Lazy use.
		//
		inline auto operator[]( const char* name ) { return table_proxy<const char*, false>( get_ref(), name ); }
		inline auto operator[]( size_t idx ) { return table_proxy<size_t, false>( get_ref(), idx ); }
		template<typename... Tx> inline function_result operator()( Tx&&... args ) { push(); return detail::pcall( table.state(), std::forward<Tx>( args )... ); }

		// Setter.
		//
		template<typename T>
		inline void set( T&& value )
		{
			stack::push<T>( table.state(), std::forward<T>( value ) );
			if constexpr ( Raw ) detail::put_table( table, key, raw_t{} );
			else                 detail::put_table( table, key );
		}
		template<typename T> inline table_proxy& operator=( T&& value ) { set<T>( std::forward<T>( value ) ); return *this; }
	};
	
	// Object.
	//
	template<Reference Ref>
	struct basic_object : Ref
	{
		inline basic_object() {}
		template<typename... Tx>
		explicit inline basic_object( Tx&&... ref ) : Ref( std::forward<Tx>( ref )... ) {}
		
		// Generic casts.
		//
		template<typename T> inline auto as() const { return stack::get<T>( *this ); }
		template<typename T> inline bool is() const { return stack::check<T>( *this ); }
		template<typename T> inline operator T() const { return stack::get<T>( *this ); }
		inline std::string to_string() const { return detail::to_string( *this ); }

		// Lazy use.
		//
		inline auto operator[]( const char* name ) { return table_proxy<const char*, false>( *this, name ); }
		inline auto operator[]( size_t idx ) { return table_proxy<size_t, false>( *this, idx ); }
		template<typename... Tx> inline function_result operator()( Tx&&... args ) { Ref::push(); return detail::pcall( Ref::state(), std::forward<Tx>( args )... ); }
	};
	using object =       basic_object<registry_reference>;
	using stack_object = basic_object<stack_reference>;

	// Table iterator.
	//
	struct iterator
	{
		// Define iterator traits.
		//
		using iterator_category = std::forward_iterator_tag;
		using difference_type =   stack::slot;
		using value_type =        std::pair<object, object>;
		using reference =         const value_type&;
		using pointer =           const value_type*;

		stack_reference table = {};
		std::pair<object, object> at = {};
		bool end = false;
		
		inline iterator( stack_reference _table ) : table( std::move( _table ) )
		{ 
			stack::push( table.state(), nil );
			pop_state();
		}
		inline iterator() : end( true ) {}
		inline void pop_state()
		{
			if ( !lua_next( table.state(), table.slot() ) )
			{
				at = { object{}, object{} };
				end = true;
			}
			else
			{
				auto value = stack::pop<object>( table.state() );
				auto key = stack::pop<object>( table.state() );
				at = { std::move( key ), std::move( value ) };
			}
		}
		inline iterator& operator++() 
		{
			at.first.push();
			pop_state();
			return *this; 
		}
		inline bool operator==( const iterator& other ) const { return end == other.end; }
		inline bool operator!=( const iterator& other ) const { return end != other.end; }
		inline reference operator*() const { return at; }
		inline pointer operator->() const { return &at; }
	};

	// Table.
	//
	template<Reference Ref>
	struct basic_table : Ref, typed_reference_tag_t
	{
		inline static bool check( const stack_reference& ref ) { return lua_istable( ref.state(), ref.slot() ); }
		
		inline basic_table() {}
		template<typename... Tx>
		explicit inline basic_table( Tx&&... ref ) : Ref( std::forward<Tx>( ref )... ) {}

		// TODO: Length
		//
		inline iterator begin() const { return { *this }; }
		inline iterator end() const { return {}; }

		inline auto at( const char* name ) { return table_proxy<const char*, false>{ *this, name }; }
		inline auto at( size_t idx ) { return table_proxy<size_t, false>{ *this, idx }; }
		inline auto at( const char* name, raw_t ) { return table_proxy<const char*, true>{ *this, name }; }
		inline auto at( size_t idx, raw_t ) { return table_proxy<size_t, true>{ *this, idx }; }
		inline auto operator[]( const char* name ) { return at( name ); }
		inline auto operator[]( size_t idx ) { return at( idx ); }
	};
	using table =       basic_table<registry_reference>;
	using stack_table = basic_table<stack_reference>;

	// Function.
	//
	template<Reference Ref>
	struct basic_function : Ref, typed_reference_tag_t
	{
		inline static bool check( const stack_reference& ref ) { return lua_isfunction( ref.state(), ref.slot() ); }

		inline basic_function() {}
		template<typename... Tx>
		explicit inline basic_function( Tx&&... ref ) : Ref( std::forward<Tx>( ref )... ) {}

		template<typename... Tx> function_result invoke( Tx&&... args ) { Ref::push(); return detail::pcall( Ref::state(), std::forward<Tx>( args )... ); }
		template<typename... Tx> function_result operator()( Tx&&... args ) { Ref::push(); return detail::pcall( Ref::state(), std::forward<Tx>( args )... ); }
	};
	using function =       basic_function<registry_reference>;
	using stack_function = basic_function<stack_reference>;

	// Libraries.
	//
	using library_descriptor = std::pair<cfunction_t, const char*>;
	namespace lib
	{
		static constexpr library_descriptor base =    { &luaopen_base,     "base" };
		static constexpr library_descriptor package = { &luaopen_package,  "package" };
		static constexpr library_descriptor string =  { &luaopen_string,   "string" };
		static constexpr library_descriptor table =   { &luaopen_table,    "table" };
		static constexpr library_descriptor math =    { &luaopen_math,     "math" };
		static constexpr library_descriptor io =      { &luaopen_io,       "io" };
		static constexpr library_descriptor os =      { &luaopen_os,       "os" };
		static constexpr library_descriptor debug =   { &luaopen_debug,    "debug" };
		static constexpr library_descriptor bit =     { &luaopen_bit,      "bit32" };
		static constexpr library_descriptor ffi =     { &luaopen_ffi,      "ffi" };
		static constexpr library_descriptor jit =     { &luaopen_jit,      "jit" };
	};

	// Load result.
	//
	struct load_result
	{
		registry_reference ref;
		int loadfile_result;
		
		inline bool is_error() const { return loadfile_result != 0; }
		inline bool is_success() const { return loadfile_result == 0; }
		inline void assert() const { if ( is_error() ) detail::error( ref.state(), error() ); }
		inline explicit operator bool() const { return is_success(); }

		const char* error() const { return stack::get<const char*>( ref ); }

		template<typename... Tx>
		function_result operator()( Tx&&... args ) const
		{
			ref.push(); 
			return detail::pcall( ref.state(), std::forward<Tx>( args )... );
		}
	};

	// State.
	//
	struct state
	{
		lua_State* L;

		inline state( lua_State* state ) : L( state ) {}
		inline state() : state( luaL_newstate() ) {}
		inline ~state() { lua_close( L ); }
		operator lua_State* ( ) const { return L; }

		// Sets the panic function.
		//
		inline void set_panic( cfunction_t func ) { lua_atpanic( L, func ); }

		// Creates a table.
		//
		inline table make_table( size_t num_arr = 0, size_t num_rec = 0 )
		{
			lua_createtable( L, num_arr, num_rec );
			return table{ registry_reference{ L, stack::top_t{} } };
		}

		// Creates a metatable.
		//
		inline std::pair<table, bool> make_metatable( const char* name )
		{
			bool inserted = luaL_newmetatable( L, name ) == 1;
			return std::pair{ table{ registry_reference{ L, stack::top_t{} } }, inserted };
		}

		// References globals.
		//
		inline stack_table globals() { return stack_table{ stack_reference{ L, LUA_GLOBALSINDEX } }; }
		inline auto operator[]( const char* name ) { return table_proxy<const char*, false>( stack_reference{ L, LUA_GLOBALSINDEX }, name ); }

		// Opens the given libraries.
		//
		inline void open_libraries( const library_descriptor& desc )
		{
			lua_pushcfunction( L, desc.first );
			lua_pushstring( L, desc.second );
			lua_call( L, 1, 1 );
			lua_setglobal( L, desc.second );
		}
		template<typename... Tx>
		inline void open_libraries( Tx&&... descriptors )
		{
			( open_libraries( descriptors ), ... );
		}

		// Parses a script and returns the chunk as a function.
		// 
		inline load_result load_file( const char* path )
		{
			int r = luaL_loadfile( L, path );
			return { registry_reference{ L, stack::top_t{} }, r };
		}
		inline load_result load( std::string_view script, const char* name = "" )
		{
			int r = luaL_loadbuffer( L, script.data(), script.size(), name );
			return { registry_reference{ L, stack::top_t{} }, r };
		}

		// Runs the given script and returns any parsing errors.
		//
		inline function_result script_file( const char* path )
		{
			auto result = load( path );
			if ( !result )
			{
				stack::push( L, result.ref );
				stack::slot top = stack::top( L );
				return function_result{ L, top, top + 1, result.loadfile_result };
			}
			return result();
		}
		inline function_result script( std::string_view script, const char* name = "" )
		{
			auto result = load( script, name );
			if ( !result )
			{
				stack::push( L, result.ref );
				stack::slot top = stack::top( L );
				return function_result{ L, top, top + 1, result.loadfile_result };
			}
			return result();
		}

		// Collects garbage.
		//
		inline void collect_garbage()
		{
			lua_gc( L, LUA_GCCOLLECT, 0 );
		}
	};

	// Sets the metatable for a given object.
	//
	template<Reference Ref>
	inline static void set_metatable( const stack_reference& dst, const Ref& table )
	{
		table.push();
		lua_setmetatable( dst.state(), dst.index() );
	}
};
#pragma pack(pop)
