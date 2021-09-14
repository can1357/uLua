#pragma once
#include <memory>
#include "stack.hpp"
#include "reference.hpp"

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
		template<UserdataHasFields T> struct userdata_fields<T> { static constexpr auto& value = user_traits<T>::fields; };
	};
	template<typename T>
	static constexpr auto& userdata_fields = detail::userdata_fields<T>::value;

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
#if ULUA_NO_CTTI
			// Generate anonymous runtime type and metatable names.
			//
			inline static const std::string _metatable_name = "@unnamed_" + std::to_string( ++unnamed_type_counter );
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
		pointer,
		value,
		shared_ptr,
	};

	// Userdata wrappers.
	//
	template<typename T>
	struct userdata_wrapper
	{
		inline static void* __tag = nullptr;

		int64_t storage_type : 2;
		int64_t pointer      : 62;
		void* const tag = &__tag;

		inline userdata_wrapper() : storage_type( ( int64_t ) userdata_storage::pointer ), pointer( 0 ) {}
		inline userdata_wrapper( T* pointer, userdata_storage type ) : storage_type( ( int64_t ) type ), pointer( ( int64_t ) pointer ) {}

		inline bool check() const { return tag == &__tag; }
		inline operator T*() const { return get(); }
		inline T* get() const { return ( T* ) ( uint64_t ) ( int64_t ) pointer; }
		inline T& value() const { return *get(); }
		inline userdata_storage storage() const { return ( userdata_storage ) storage_type; }

		template<typename S> 
		inline S* store() const { return ( S* ) ( this + 1 ); }

		inline void destroy() const
		{
			switch ( storage() )
			{
				case userdata_storage::pointer:    return;
				case userdata_storage::value:      return std::destroy_at( store<T>() );
				case userdata_storage::shared_ptr: return std::destroy_at( store<std::shared_ptr<T>>() );
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
	template<typename T>
	struct userdata_by_shared_ptr : userdata_wrapper<T>
	{
		std::shared_ptr<T> value;
		inline userdata_by_shared_ptr( std::shared_ptr<T> value ) : userdata_wrapper<T>( value.get(), userdata_storage::value ), value( std::move( value ) ) {}
	};

	// Implement type traits.
	//
	template<typename T>
	struct type_traits<userdata_wrapper<T>>
	{
		// No pusher.
		inline static bool check( lua_State* L, int idx ) { return true; }
		inline static userdata_wrapper<T>& get( lua_State* L, int idx ) { return *( userdata_wrapper<T>* ) lua_touserdata( L, idx ); }
	};
	template<UserType T>
	struct type_traits<T>
	{
		template<typename V = T>
		inline static void push( lua_State* L, V&& value )
		{
			stack::emplace_userdata<userdata_by_value<T>>( L, std::forward<V>( value ) );
			userdata_metatable<T>::push( L );
			stack::set_metatable( L, -2 );
		}
		inline static bool check( lua_State* L, int idx )
		{
			void* p = lua_touserdata( L, idx );
			return p && ( ( userdata_wrapper<T>* )p )->check();
		}
		inline static T& get( lua_State* L, int idx )
		{
			auto& wrapper = stack::get<userdata_wrapper<T>>( L, idx );
			if ( !wrapper.check() )
				detail::type_error( L, idx, userdata_name<T>().data() );
			return wrapper.value();
		}
	};
	template<UserType T>
	struct type_traits<T*>
	{
		inline static void push( lua_State* L, T* pointer )
		{
			stack::emplace_userdata<userdata_by_pointer<T>>( L, pointer );
			userdata_metatable<T>::push( L );
			stack::set_metatable( L, -2 );
		}
		inline static T* get( lua_State* L, int idx )
		{
			T& result = type_traits<T>::get( L, idx );
			return &result;
		}
		inline static bool check( lua_State* L, int idx ) { return type_traits<T>::check( L, idx ); }
	};
	template<UserType T>
	struct type_traits<std::shared_ptr<T>>
	{
		inline static void push( lua_State* L, std::shared_ptr<T> pointer )
		{
			stack::emplace_userdata<userdata_by_shared_ptr<T>>( L, std::move( pointer ) );
			userdata_metatable<T>::push( L );
			stack::set_metatable( L, -2 );
		}
		inline static std::shared_ptr<T> get( lua_State* L, int idx )
		{
			stack::copy( L, i );
			reg_key key = stack::pop_reg( L );

			T& result = type_traits<T>::get( L, idx );
			return std::shared_ptr<T>( &result, [ L, key ] () { unref( L, key ); } );
		}
		inline static bool check( lua_State* L, int idx ) { return type_traits<T>::check( L, idx ); }
		// No getter.
	};
	template<UserType T>
	struct type_traits<std::reference_wrapper<T>>
	{
		inline static void push( lua_State* L, std::reference_wrapper<T> pointer )
		{
			stack::emplace_userdata<userdata_by_pointer<T>>( L, &pointer.get() );
			userdata_metatable<T>::push( L );
			stack::set_metatable( L, -2 );
		}
		inline static std::reference_wrapper<T> get( lua_State* L, int idx )
		{
			T& result = type_traits<T>::get( L, idx );
			return std::reference_wrapper<T>( result );
		}
		inline static bool check( lua_State* L, int idx ) { return type_traits<T>::check( L, idx ); }
	};
};