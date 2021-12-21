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
		inline static bool check( lua_State*, int& slot ) { slot++; return true; }
	};
	template<typename T>
	concept Reference = std::is_base_of_v<reference_base, std::decay_t<T>>;

	// Null reference.
	//
	struct nullref_t
	{ 
		struct tag {}; 
		constexpr explicit nullref_t( tag ) {} 
		constexpr bool operator==( nullref_t ) const noexcept { return true; }
		constexpr bool operator!=( nullref_t ) const noexcept { return false; }
	};
	inline constexpr nullref_t nullref{ nullref_t::tag{} };

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
		inline constexpr stack_reference( nullref_t ) {}
		inline constexpr stack_reference( lua_State* L, int index ) : L( L ), index( stack::abs( L, index ) ), valid_flag( true ), ownership_flag( true ) {}
		inline constexpr stack_reference( lua_State* L, int index, weak_t ) : L( L ), index( stack::abs( L, index ) ), valid_flag( true ), ownership_flag( false ) {}

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

		// Comparison against nullref.
		//
		inline constexpr bool operator!=( nullref_t ) const { return valid(); }
		inline constexpr bool operator==( nullref_t ) const { return !valid(); }

		// Generic reference functions.
		//
		inline constexpr lua_State* state() const { return L; }
		inline constexpr int slot() const { return index; }
		inline void push() const { lua_pushvalue( state(), slot() ); }
		inline constexpr bool valid() const { return valid_flag; }
		inline constexpr void release() { valid_flag = false; }
		inline constexpr void reset()
		{
			if ( std::exchange( valid_flag, false ) && std::exchange( ownership_flag, false ) && stack::is_absolute( index ) )
				stack::checked_remove( L, index );
		}
		inline constexpr void reset( detail::unchecked_t )
		{
			if ( std::exchange( valid_flag, false ) && std::exchange( ownership_flag, false ) && stack::is_absolute( index ) )
				stack::remove( L, index );
		}
		inline constexpr ~stack_reference() { reset(); }
	};
	struct registry_reference : reference_base
	{
		lua_State* L = nullptr;
		reg_key key = {};
		bool valid_flag = false;

		// Construction, copy & move.
		//
		inline constexpr registry_reference() {}
		inline constexpr registry_reference( nullref_t ) {}
		inline constexpr registry_reference( lua_State* L, reg_key key ) : L( L ), key( key ), valid_flag( true ) {}
		inline constexpr registry_reference( lua_State* L, nil_t ) : L( L ), key{ LUA_REFNIL }, valid_flag( true ) {}
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

		// Comparison against nullref.
		//
		inline constexpr bool operator!=( nullref_t ) const { return valid(); }
		inline constexpr bool operator==( nullref_t ) const { return !valid(); }

		// Generic reference functions.
		//
		inline constexpr lua_State* state() const { return L; }
		inline constexpr reg_key registry_key() const { return key; }
		inline void push() const { stack::push_reg( L, key ); }
		inline constexpr bool valid() const { return valid_flag; }
		inline constexpr void release() { valid_flag = false; }
		inline constexpr void reset()
		{
			if ( std::exchange( valid_flag, false ) )
				unref( L, key );
		}
		inline constexpr void reset( detail::unchecked_t ) { reset(); }
		inline constexpr ~registry_reference() { reset(); }
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

	// Compares two references for equality.
	//
	inline bool equals( lua_State* L, const stack_reference& r1, const stack_reference& r2 )
	{
		return stack::equals( L, r1.slot(), r2.slot() );
	}
	inline bool equals( lua_State* L, const stack_reference& r1, const registry_reference& r2 )
	{
		auto s1 = stack::abs( L, r1.slot() );
		r2.push();
		bool result = stack::equals( L, s1, stack::top_t{} );
		stack::pop_n( L, 1 );
		return result;
	}
	inline bool equals( lua_State* L, const registry_reference& r1, const stack_reference& r2 )
	{
		auto s2 = stack::abs( L, r2.slot() );
		r1.push();
		bool result = stack::equals( L, stack::top_t{}, s2 );
		stack::pop_n( L, 1 );
		return result;
	}
	inline bool equals( lua_State* L, const registry_reference& r1, const registry_reference& r2 )
	{
		r2.push();
		r1.push();
		bool result = stack::equals( L, -1, -2 );
		stack::pop_n( L, 2 );
		return result;
	}
};