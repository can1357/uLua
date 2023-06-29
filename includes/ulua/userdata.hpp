#pragma once
#include <memory>
#include "stack.hpp"
#include "reference.hpp"

#ifndef ULUA_CONST_CORRECT
	#define ULUA_CONST_CORRECT 0
#endif

namespace ulua
{
	// User defined type traits.
	//
	template<typename T>
	struct user_traits : nil_t {};
	template<typename T>
	concept UserType = ( !std::is_base_of_v<nil_t, user_traits<T>> );
	template<typename T>
	struct userdata_metatable;

	// Userdata fields.
	//
	namespace detail
	{
		template<typename T>
		concept UserdataHasFields = requires{ user_traits<T>::fields; };
		template<typename T> struct userdata_fields { static constexpr std::tuple<> value = {}; };
		template<UserdataHasFields T> struct userdata_fields<T> { static constexpr const auto& value = user_traits<T>::fields; };

		template<typename T>
		concept UserdataHasMetatable = requires{ user_traits<T>::metatable; };
		template<typename T> struct userdata_metatable { static constexpr std::tuple<> value = {}; };
		template<UserdataHasMetatable T> struct userdata_metatable<T> { static constexpr const auto& value = user_traits<T>::metatable; };
	};
	template<typename T> static constexpr const auto& userdata_fields = detail::userdata_fields<T>::value;
	template<typename T> static constexpr const auto& userdata_meta = detail::userdata_metatable<T>::value;

	// Userdata naming.
	//
	namespace detail
	{
		inline size_t unnamed_type_counter = 0;

		template<typename T>
		concept NamedUsedType = requires{ ( std::string_view ) user_traits<T>::name; };

		// Name generation.
		//
		template<typename T>
		struct userdata_namer
		{
#if ULUA_CTTI
			// Generate anonymous runtime type and metatable names.
			//
			inline static const std::string _metatable_name = "@anon" + std::to_string( ++unnamed_type_counter );
			static std::string_view name() { return { _metatable_name.begin() + 2, _metatable_name.end() }; }
			static std::string_view metatable_name() { return { _metatable_name.begin(), _metatable_name.end() }; }
#else
			// Generate automatic compile time type and metatable names.
			//
			static constexpr auto _name = [ ] ()
			{
				constexpr std::string_view name = ctti_namer<T>{};
				std::array<char, name.size() + 1> array = { 0 };
				std::copy( name.begin(), name.end(), array.data() );
				return array;
			}( );
			static constexpr auto _metatable_name = [ ] ()
			{
				constexpr std::string_view name = ctti_namer<T>{};
				std::array<char, name.size() + 2> array = { 0 };
				std::copy( name.begin(), name.end(), array.data() + 1 );
				array[ 0 ] = '@';
				return array;
			}( );
			static constexpr std::string_view name() { return { _name.data(), _name.data() + _name.size() - 1 }; }
			static constexpr std::string_view metatable_name() { return { _metatable_name.data(), _metatable_name.data() + _metatable_name.size() - 1 }; }
#endif
		};
		template<NamedUsedType T>
		struct userdata_namer<T>
		{
			// Generate static type and metatable names.
			//
			static constexpr auto _name = [ ] ()
			{
				constexpr std::string_view name = user_traits<T>::name;
				std::array<char, name.size() + 1> array = { 0 };
				std::copy( name.begin(), name.end(), array.data() );
				return array;
			}( );
			static constexpr auto _metatable_name = [ ] ()
			{
				constexpr std::string_view name = user_traits<T>::name;
				std::array<char, name.size() + 2> array = { 0 };
				std::copy( name.begin(), name.end(), array.data() +1 );
				array[ 0 ] = '@';
				return array;
			}( );
			static constexpr std::string_view name() { return { _name.data(), _name.data() + _name.size() - 1 }; }
			static constexpr std::string_view metatable_name() { return { _metatable_name.data(), _metatable_name.data() + _metatable_name.size() - 1 }; }
		};
	};
	template<typename T> static constexpr std::string_view userdata_name() { return detail::userdata_namer<T>::name(); }
	template<typename T> static constexpr std::string_view userdata_mt_name() { return detail::userdata_namer<T>::metatable_name(); }

	// User data storage types.
	//
	enum class userdata_storage : uint8_t
	{
		pointer    = 0b00,
		value      = 0b01,
	};

	// Userdata wrappers.
	//
	template<typename T>
	inline bool __userdata_tag = false;
	
	template<typename T>
	struct userdata_wrapper
	{
		inline static uint32_t make_tag() { return ( uint32_t ) ( uint64_t ) &__userdata_tag<std::remove_const_t<T>>; }

		T*        pointer = nullptr;
		uint64_t  tag          : 32 = make_tag();
#if ULUA_CONST_CORRECT
		uint64_t  is_const     : 1 =  std::is_const_v<T>;
#endif
		uint64_t  storage_type : 1 =  0;

		inline userdata_wrapper() : pointer( nullptr ), tag( 0 ) {}
		inline userdata_wrapper( T* pointer, userdata_storage type ) : pointer( pointer ), storage_type( ( int64_t ) type ) {}

		inline bool check_type() const { return tag == make_tag(); }
		inline bool check_life() const { return pointer != nullptr; }
#if ULUA_CONST_CORRECT
		inline bool check_qual() const { return std::is_const_v<T> || !is_const; }
#else
		inline constexpr bool check_qual() const { return true; }
#endif
		inline void retire() { pointer = nullptr; }

		inline operator T*() const { return pointer; }
		inline T* get() const { return pointer; }
		inline T& value() const { return *pointer; }
		inline userdata_storage storage() const { return ( userdata_storage ) storage_type; }

		template<typename S> 
		inline S* store() const { return ( S* ) ( this + 1 ); }

		inline void destroy()
		{
			switch ( storage() )
			{
				case userdata_storage::pointer:    return;
				case userdata_storage::value:      return std::destroy_at( store<T>() );
				default:                           detail::assume_unreachable();
			}
		}
	};
	template<typename T>
	struct userdata_by_value : userdata_wrapper<T>
	{
		T value;
		template<typename... Tx>
		inline userdata_by_value( Tx&&... args ) : userdata_wrapper<T>( &value, userdata_storage::value ), value( std::forward<Tx>( args )... ) {}
	};
	template<typename T>
	struct userdata_by_pointer : userdata_wrapper<T>
	{
		inline userdata_by_pointer( T* pointer ) : userdata_wrapper<T>( pointer, userdata_storage::pointer ) {}
	};
	
	// Implement type traits.
	//
	template<typename T>
	struct type_traits<userdata_wrapper<T>>
	{
		// No pusher.
		ULUA_INLINE inline static bool check( lua_State* L, int& idx )
		{ 
			auto wrapper = ( userdata_wrapper<T>* ) type_traits<userdata_value>::get( L, idx ).pointer;
			return wrapper && wrapper->check_type() && wrapper->check_qual();
		}
		ULUA_INLINE inline static userdata_wrapper<T>& get( lua_State* L, int& idx )
		{
			int i = idx;
			auto wrapper = ( userdata_wrapper<T>* ) type_traits<userdata_value>::get( L, idx ).pointer;
			constexpr auto udname = userdata_name<std::remove_const_t<T>>().data();
			if ( !wrapper || !wrapper->check_type() ) [[unlikely]]
				type_error( L, i, udname );
#if ULUA_CONST_CORRECT
			if ( !wrapper->check_qual() ) [[unlikely]]
				arg_error( L, i, "expired mutable, got constant %s", udname );
#endif
			if ( !wrapper->check_life() ) [[unlikely]]
				arg_error( L, i, "received expired %s", udname );
			return *wrapper;
		}
	};
	template<typename T>
	struct user_type_traits : emplacable_tag_t
	{
		template<typename... Tx>
		ULUA_INLINE inline static int emplace( lua_State* L, Tx&&... args )
		{
			stack::emplace_userdata<userdata_by_value<std::remove_const_t<T>>>( L, std::forward<Tx>( args )... );
			userdata_metatable<std::remove_const_t<T>>::push( L );
			stack::set_metatable( L, -2 );
			return 1;
		}
		template<typename V = T>
		ULUA_INLINE inline static int push( lua_State* L, V&& value )
		{
			return emplace( L, std::forward<V>( value ) );
		}
		ULUA_INLINE inline static bool check( lua_State* L, int& idx )
		{
			return type_traits<userdata_wrapper<T>>::check( L, idx );
		}
		ULUA_INLINE inline static std::reference_wrapper<T> get( lua_State* L, int& idx )
		{
			return type_traits<userdata_wrapper<T>>::get( L, idx ).value();
		}
	};
	template<typename T>
	struct user_type_traits<T&&> : user_type_traits<T>
	{
		ULUA_INLINE inline static T get( lua_State* L, int& idx )
		{
			return user_type_traits<T>::get( L, idx );
		}
	};
	template<typename T>
	struct user_type_traits<T*>
	{
		ULUA_INLINE inline static int push( lua_State* L, T* pointer )
		{
			stack::emplace_userdata<userdata_by_pointer<T>>( L, pointer );
			userdata_metatable<std::remove_const_t<T>>::push( L );
			stack::set_metatable( L, -2 );
			return 1;
		}
		ULUA_INLINE inline static T* get( lua_State* L, int& idx )
		{
			T& result = user_type_traits<T>::get( L, idx );
			return &result;
		}
		ULUA_INLINE inline static bool check( lua_State* L, int& idx ) { return user_type_traits<T>::check( L, idx ); }
	};
	template<UserType T> struct type_traits<T&&> :                                        user_type_traits<T&&> {};
	template<UserType T> struct type_traits<T&> :                                         user_type_traits<T> {};
	template<UserType T> struct type_traits<T> :                                          user_type_traits<const T> {};
	template<UserType T> struct type_traits<const T&> :                                   user_type_traits<const T> {};
	template<UserType T> struct type_traits<const T&&> :                                  user_type_traits<const T> {};
	template<UserType T> struct type_traits<T*> :                                         user_type_traits<T*> {};
	template<UserType T> struct type_traits<const T*> :                                   user_type_traits<const T*> {};
	template<UserType T> struct type_traits<std::reference_wrapper<T>> :                  user_type_traits<T> {};
	template<UserType T> struct type_traits<std::reference_wrapper<const T>> :            user_type_traits<const T> {};
};