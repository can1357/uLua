#pragma once
#include <atomic>
#include "stack.hpp"
#include "lazy.hpp"
#include "table.hpp"
#include "state.hpp"
#include "userdata.hpp"

#ifndef ULUA_CTYPE_VALIDATED
	#define ULUA_CTYPE_VALIDATED 0
#endif

#if ULUA_JIT
namespace ulua::ffi
{
	// Gets the typeid of a given type name.
	//
	inline CTypeID typeid_of( lua_State* state, const char* name )
	{
		constexpr uint32_t mask = ( 1u << CT_KW ) | ( 1u << CT_STRUCT ) | ( 1u << CT_ENUM );

		stack::push( state, name );
		CType* t;
		CTypeID id = lj_ctype_getname( ctype_cts( state ), &t, strV( accel::ref( state, -1 ) ), mask );
		stack::pop_n( state, 1 );
		return id;
	}

	// Parses the given C code in the state view.
	//
	inline function_result cdef( state_view state, std::string_view src )
	{
		stack_table t{ state, LUA_REGISTRYINDEX, weak_t{} };
		return t[ "_LOADED" ][ "ffi" ][ "cdef" ]( src );
	}

	// Sets the metatable for a given type name.
	//
	inline function_result set_metatable( state_view state, const char* type_name, const table& tbl )
	{
		stack_table t{ state, LUA_REGISTRYINDEX, weak_t{} };
		return t[ "_LOADED" ][ "ffi" ][ "metatype" ]( type_name, tbl );
	}
};

namespace ulua
{
	// C type traits.
	//
	struct ctype_t {};
	template<typename T>
	concept UserCType = std::is_base_of_v<ctype_t, user_traits<T>>;
	template<typename T>
	concept CTypeHasInlineCDef = requires { T::cdef; };

	// Replace userdata wrapper.
	//
	template<typename T> requires UserCType<std::remove_const_t<T>>
	struct userdata_wrapper<T>
	{
		inline static uint32_t make_tag() { return ( uint32_t ) ( uint64_t ) &__userdata_tag<std::remove_const_t<T>>; }

		mutable std::remove_const_t<T> storage;
		template<typename... Tx>
		constexpr userdata_wrapper( Tx&&... args ) : storage( std::forward<Tx>( args )... ) {}

		inline constexpr bool check_type() const { return true; }
		inline constexpr bool check_qual() const { return true; }

		inline constexpr T* get() const { return &storage; }
		inline constexpr operator T*() const { return get(); }
		inline constexpr T& value() const { return *get(); }

		inline constexpr void destroy() const { std::destroy_at( &storage ); }
	};

	// Replace userdata_by_value.
	//
	template<typename T> requires UserCType<std::remove_const_t<T>>
	struct userdata_by_value<T> : userdata_wrapper<T>
	{
		using userdata_wrapper<T>::userdata_wrapper;
	};

	// Disallow userdata_by_pointer.
	//
	template<typename T> requires UserCType<std::remove_const_t<T>>
	struct userdata_by_pointer<T>
	{
		userdata_by_pointer() = delete;
	};

	// Ctype cache.
	//
	template<typename T>
	struct ctype_id_cache
	{
		using meta =  userdata_metatable<T>;

		inline static std::atomic<uint64_t> value = {};

		inline static void write( lua_State* L, CTypeID in )
		{
			uint64_t v = uint32_t( ( uint64_t ) L );
			v |= uint64_t( in ) << 32;
			value.exchange( v );
		}
		inline static bool read( lua_State* L, CTypeID& out )
		{
			uint64_t v = value.load( std::memory_order::relaxed );
			if ( uint32_t( v ) != uint32_t( ( uint64_t ) L ) ) [[unlikely]]
				return false;
			out = uint32_t( v >> 32 );
			return true;
		}
		ULUA_COLD static CTypeID fetch_slow( lua_State* L )
		{
			CTypeID type_id;
			if ( stack::create_metatable( L, userdata_mt_name<T>().data() ) ) [[unlikely]]
			{
				// Create the C definition.
				if constexpr ( CTypeHasInlineCDef<user_traits<T>> )
					ffi::cdef( L, user_traits<T>::cdef ).assert();
				// Get the type ID.
				type_id = ffi::typeid_of( L, userdata_name<T>().data() );
				// Setup the metatable.
				meta::setup( L, stack::top_t{} );
				// Add the __cid property.
				ulua::table table{ L, stack::top_t{} };
				table[ "i" ] = type_id;
				// Set the type metatable.
				ffi::set_metatable( L, userdata_name<T>().data(), table );
			}
			else
			{
				// Read the __cid property.
				ulua::stack_table table{ L, stack::top_t{} };
				type_id = table[ "i" ];
			}
			// Update cache.
			write( L, type_id );
			return type_id;
		}
		ULUA_INLINE static CTypeID fetch( lua_State* L )
		{
			CTypeID type_id;
			if ( !read( L, type_id ) ) [[unlikely]]
				type_id = fetch_slow( L );
			return type_id;
		}
	};

	// Replace type traits of the userdata.
	//
	template<typename T> requires UserCType<std::remove_const_t<T>>
	struct type_traits<userdata_wrapper<T>>
	{
		using cache = ctype_id_cache<std::remove_const_t<T>>;

		// No pusher.
		ULUA_INLINE static bool check( lua_State* L, int& idx )
		{ 
			auto* tv = accel::ref( L, idx++ );
			if ( !tviscdata( tv ) )
				return false;
			return cdataV( tv )->ctypeid == cache::fetch( L );
		}
		ULUA_INLINE static userdata_wrapper<T>& get( lua_State* L, int& idx )
		{
			auto* tv = accel::ref( L, idx++ );
			if ( !tviscdata( tv ) )
				type_error( L, idx - 1, userdata_name<std::remove_const_t<T>>().data() );
			auto* cd = cdataV( tv );
			auto* wr = ( userdata_wrapper<T>* ) cdataptr( cd );

#if ULUA_CTYPE_VALIDATED
			if ( cd->ctypeid != cache::fetch( L ) )
				type_error( L, idx - 1, userdata_name<std::remove_const_t<T>>().data() );
#endif
			return *wr;
		}
	};
	template<typename T> requires UserCType<std::remove_const_t<T>>
	struct user_type_traits<T> : emplacable_tag_t
	{
		using cache = ctype_id_cache<std::remove_const_t<T>>;

		template<typename... Tx>
		ULUA_INLINE static int emplace( lua_State* L, Tx&&... args )
		{
			// Emplace the C type.
			//
			auto* cd = lj_cdata_new_( L, cache::fetch( L ), sizeof( userdata_wrapper<T> ) );
			setcdataV( L, L->top, cd );
			incr_top( L );
			new ( cdataptr( cd ) ) userdata_wrapper<T>( std::forward<Tx>( args )... );
			return 1;
		}
		template<typename V = T>
		ULUA_INLINE static int push( lua_State* L, V&& value )
		{
			return emplace( L, std::forward<V>( value ) );
		}
		ULUA_INLINE static bool check( lua_State* L, int& idx )
		{
			return type_traits<userdata_wrapper<T>>::check( L, idx );
		}
		ULUA_INLINE static std::reference_wrapper<T> get( lua_State* L, int& idx )
		{
			return type_traits<userdata_wrapper<T>>::get( L, idx ).value();
		}
	};
};
#endif