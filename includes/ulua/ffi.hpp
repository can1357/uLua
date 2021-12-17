#pragma once
#include "stack.hpp"
#include "lazy.hpp"
#include "table.hpp"
#include "state.hpp"

#if ULUA_JIT
namespace ulua::ffi
{
	// Parses the given C code in the state view, returns the error code.
	//
	inline int cdef( lua_State* state, const char* src )
	{
		stack::push( state, src );

		GCstr* s = strV( accel::ref( state, -1 ) );
		CPState cp;
		int errcode;
		cp.L = state;
		cp.cts = ctype_cts( state );
		cp.srcname = strdata( s );
		cp.p = strdata( s );
		cp.param = cp.L->base + 1;
		cp.mode = CPARSE_MODE_MULTI | CPARSE_MODE_DIRECT;
		errcode = lj_cparse( &cp );

		stack::pop_n( state, 1 );
		return errcode;
	}

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

	// Sets the metatable for a given type name.
	//
	inline function_result set_metatable( state_view state, const char* type_name, const table& tbl )
	{
		return state[ "ffi" ][ "metatype" ]( type_name, tbl );
	}

	// C value wrapper.
	//
	template<typename T, Reference Ref>
	struct basic_cvalue : Ref
	{
		inline constexpr basic_cvalue() {}
		template<typename... Tx> requires( sizeof...( Tx ) != 0 && detail::Constructible<Ref, Tx...> )
		explicit inline constexpr basic_cvalue( Tx&&... ref ) : Ref( std::forward<Tx>( ref )... ) {}

		// Creating constructor.
		//
		template<typename... Tx>
		inline basic_cvalue( lua_State* L, create, CTypeID id, Tx&&... args )
		{
			CTState* cts = ctype_cts( L );
			GCcdata* cd = lj_cdata_new( cts, id, sizeof( T )  );

			setcdataV( L, L->top, cd );
			incr_top( L );

			Ref ref{ L, stack::top_t{} };
			Ref::swap( ref );

			new ( cdataptr( cd ) ) T( std::forward<Tx>( args )... );
		}

		// Gets the type id.
		//
		CTypeID get_typeid() const 
		{ 
			if constexpr ( Ref::is_direct )
			{
				return cdataV( accel::ref( Ref::L, Ref::slot() ) )->ctypeid;
			}
			else
			{
				Ref::push();
				auto r = cdataV( accel::ref( Ref::L, stack::top_t{} ) )->ctypeid;
				stack::pop_n( Ref::L, 1 );
				return r;
			}
		}

		// Gets the raw pointer.
		//
		T* get_pointer() const 
		{
			if constexpr ( Ref::is_direct )
			{
				return ( T* ) cdataptr( cdataV( accel::ref( Ref::L, Ref::slot() ) ) );
			}
			else
			{
				Ref::push();
				auto r = ( T* ) cdataptr( cdataV( accel::ref( Ref::L, stack::top_t{} ) ) );
				stack::pop_n( Ref::L, 1 );
				return r;
			}
		}
		T& operator*() const { return *get_pointer(); }
		T* operator->() const { return get_pointer(); }
	};
	
	template<typename T>
	using cvalue =       basic_cvalue<T, registry_reference>;
	template<typename T>
	using stack_cvalue = basic_cvalue<T, stack_reference>;
};
#endif