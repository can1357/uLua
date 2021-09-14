#pragma once
#include "common.hpp"
#include "stack.hpp"

namespace ulua
{
	namespace detail { struct unchecked_t {}; };

	// Reference tag.
	//
	struct reference_base
	{
		static constexpr bool is_direct = false;
		static constexpr bool owning = false;

		inline static bool check( lua_State*, int& slot ) { slot++; return true; }
	};
	template<typename T>
	concept Reference = std::is_base_of_v<reference_base, std::decay_t<T>>;

	// Reference type.
	//
	struct weak_t {};
	struct stack_reference : reference_base
	{
		static constexpr bool is_direct = true;

		lua_State* L = nullptr;
		int index = 0;
		bool valid_flag = false;
		bool ownership_flag = false;

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
				stack::checked_remove( L, index );
		}
		inline void reset( detail::unchecked_t )
		{
			if ( std::exchange( valid_flag, false ) && std::exchange( ownership_flag, false ) && stack::is_absolute( index ) )
				stack::remove( L, index );
		}
		inline ~stack_reference() { reset(); }
	};
	struct registry_reference : reference_base
	{
		lua_State* L = nullptr;
		reg_key key = {};
		bool valid_flag = false;

		// Construction, copy & move.
		//
		inline constexpr registry_reference() {}
		inline constexpr registry_reference( lua_State* L, reg_key key ) : L( L ), key( key ), valid_flag( true ) {}
		inline registry_reference( lua_State* L, stack::top_t ) : registry_reference( L, stack::pop_reg( L ) ) {}

		inline constexpr registry_reference( registry_reference&& o ) noexcept { swap( o ); }
		inline constexpr registry_reference& operator=( registry_reference&& o ) noexcept { swap( o ); return *this; }
		template<Reference Ref> inline registry_reference( const Ref& o ) { assign( o ); }
		template<Reference Ref> inline registry_reference& operator=( const Ref& o ) { assign( o ); return *this; }
		inline constexpr void swap( registry_reference& other ) noexcept
		{
			std::swap( L, other.L );
			std::swap( key.key, other.key.key );
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
				key = stack::pop_reg( L );
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
		inline reg_key registry_key() const { return key; }
		inline void push() const { stack::push_reg( L, key ); }
		inline bool valid() const { return valid_flag; }
		inline void release() { valid_flag = false; }
		inline void reset()
		{
			if ( std::exchange( valid_flag, false ) )
				unref( L, key );
		}
		inline ~registry_reference() { reset(); }
	};

	// Declare type traits for reference types.
	//
	template<Reference R>
	struct type_traits<R> : popable_tag_t
	{
		inline static int push( lua_State* L, const R& ref ) { ref.push(); return 1; }
		inline static bool check( lua_State* L, int& idx ) { return R::check( L, idx ); }
		inline static R get( lua_State* L, int& idx ) 
		{ 
			if constexpr ( R::is_direct )
			{
				return R{ L, idx++, weak_t{} };
			}
			else
			{
				stack::copy( L, idx++ );
				return R{ L, stack::top_t{} };
			}
		}
		inline static R pop( lua_State* L ) { return R{ L, stack::top_t{} }; }
	};
};