#pragma once
#include <utility>
#include <string_view>
#include "common.hpp"
#include "userdata.hpp"
#include "closure.hpp"
#include "table.hpp"

namespace ulua
{
	namespace detail
	{
		template<typename T> concept HasMappedType = requires{ typename T::mapped_type; };
		template<typename T> concept HasValueType = requires{ typename T::value_type; };
		template<typename T> concept HasKeyType = requires{ typename T::key_type; };

		template<typename T>   struct default_key_type    { using type = size_t; };
		template<HasKeyType T> struct default_key_type<T> { using type = typename T::key_type; };
		template<typename T>   
		using default_key_type_t = typename default_key_type<T>::type;

		template<typename T>
		struct default_value_type;
		template<HasValueType T> requires ( !HasMappedType<T> )
		struct default_value_type<T> { using type = typename T::value_type; };
		template<HasMappedType T> 
		struct default_value_type<T> { using type = typename T::mapped_type; };
		template<typename T>
		using default_value_type_t = typename default_value_type<T>::type;

		template<typename T> concept HasToString = requires( const T& v ) { v.to_string(); };
		template<typename T> concept HasLength = requires( const T& v ) { v.length(); };
		template<typename T> concept HasSize = requires( const T& v ) { std::size( v ); };
		template<typename T> concept EqComparable = requires( const T& a, const T& b ) { a == b; };
		template<typename T> concept GeComparable = requires( const T& a, const T& b ) { a >= b; };
		template<typename T> concept GtComparable = requires( const T& a, const T& b ) { a > b; };
		template<typename T> concept LeComparable = requires( const T& a, const T& b ) { a <= b; };
		template<typename T> concept LtComparable = requires( const T& a, const T& b ) { a < b; };
		template<typename T> concept Iterable = requires( const T& v ) { std::begin( v ); std::end( v ); };
		template<typename T> concept KvIterable = requires( const T& v ) { std::begin( v )->first; std::begin( v )->second; };
		template<typename T> concept Indexable = requires( T& v ) { v[ std::declval<default_key_type_t<T>>() ]; };
		template<typename T> concept NewIndexable = requires( T& v ) { v[ std::declval<default_key_type_t<T>>() ] = std::declval<default_value_type_t<T>>(); };
		template<typename T> concept Negable = requires( const T& v ) { -v; };
		template<typename T, typename O> concept Addable = requires( const T& v, const O& v2 ) { v + v2; };
		template<typename T, typename O> concept Subable = requires( const T& v, const O& v2 ) { v - v2; };
		template<typename T, typename O> concept Mulable = requires( const T& v, const O& v2 ) { v * v2; };
		template<typename T, typename O> concept Divable = requires( const T& v, const O& v2 ) { v / v2; };
		template<typename T, typename O> concept Idivable = requires( const T& v, const O& v2 ) { size_t( v / v2 ); };
		template<typename T, typename O> concept Modable = requires( const T& v, const O& v2 ) { v% v2; };
		template<typename T, typename O> concept Powable = requires( const T& v, const O& v2 ) { pow( v, v2 ); };
	};

	// Helpers for the user to expose members or properties.
	//
	template<auto Value>
	struct member 
	{ 
		std::string_view name;

		template<typename T>
		static inline decltype( auto ) get( T* ptr ) 
		{
			if constexpr ( detail::is_member_function_v<decltype( Value )> )
			{
				return detail::constant<Value>();
			}
			else
			{
				return ptr->*Value;
			}
		}
		template<typename T>
		static inline bool set( T* ptr, const stack_object& ref )
		{ 
			if constexpr ( detail::is_member_function_v<decltype( Value )> )
			{
				return false;
			}
			else
			{
				ptr->*Value = ref;
				return true;
			}
		}
	};
	template<auto Getter, auto Setter = nil>
	struct property 
	{ 
		std::string_view name; 

		template<typename T>
		static inline decltype(auto) get( T* ptr ) 
		{
			return ( ptr->*Getter ) ( );
		}
		template<typename T>
		static inline bool set( T* ptr, const stack_object& ref )
		{
			if constexpr ( !std::is_same_v<decltype( Setter ), nil_t> )
			{
				ptr->*Setter( ref );
				return true;
			}
			return false;
		}
	};

	// Define the auto generated userdata metatable.
	//
	template<typename T>
	struct userdata_metatable
	{
		// Compute the maximum field length for the swith case generation.
		//
		static constexpr size_t max_field_length = [ ] ()
		{
			size_t n = 0;
			detail::enum_tuple( userdata_fields<T>, [ & ] ( const auto& field )
			{
				n = std::max<size_t>( field.name.size(), n );
			} );
			return n;
		}();

		// Implement a field finding helper.
		//
		template<typename F>
		static constexpr bool find_field( std::string_view name, F&& fn )
		{
			if ( size_t i = name.size(); i <= max_field_length )
			{
				return detail::visit_index<max_field_length + 1>( i, [ & ] <size_t N> ( detail::const_tag<N> ) FORCE_INLINE
				{
					bool found = false;
					detail::enum_tuple( userdata_fields<T>, [ & ] ( const auto& field )
					{
						if ( field.name.size() == N )
						{
							if ( !found && detail::const_eq<N>( field.name.data(), name.data() ) )
							{
								fn( field );
								found = true;
							}
						}
					} );
					return found;
				} );
			}
			return false;
		}
		template<typename F>
		static constexpr void find_field_or_die( lua_State* L, std::string_view name, F&& fn )
		{
			if ( !find_field( name, std::forward<F>( fn ) ) )
				detail::error( L, "attempt to index undefined field '%.*s' for type '%.*s'", name.length(), name.data(), userdata_name<T>().length(), userdata_name<T>().data() );
		}

		// Indexing of the object.
		//
		static push_count index( lua_State* L, const userdata_wrapper<T>& u, const stack_object& k )
		{
			auto field_indexer = [ & ] <typename Field> ( const Field & field ) { stack::push( L, Field::get( u.get() ) ); };

			if constexpr ( detail::Indexable<T> )
			{
				using K = detail::default_key_type_t<T>;

				bool is_string = k.is<std::string_view>();
				if ( is_string )
				{
					if ( find_field( k.as<std::string_view>(), field_indexer ) )
						return { 1 };
				}
				if ( std::is_same_v<K, const char*> || std::is_same_v<K, std::string_view> || std::is_same_v<K, std::string> ? is_string : k.is<K>() )
					stack::push( L, u.value()[ k.as<K>() ] );
				else
					stack::push( L, nil );
				return { 1 };
			}
			else
			{
				find_field_or_die( L, k.as<std::string_view>(), field_indexer );
				return { 1 };
			}
		}
		static void newindex( lua_State* L, const userdata_wrapper<T>& u, const stack_object& k, const stack_object& v )
		{
			auto field_indexer = [ & ] <typename Field> ( const Field & field )
			{
				if ( !Field::set( u.get(), v ) )
					detail::error( L, "attempt to modify constant field '%.*s' for type '%.*s'", field.name.length(), field.name.data(), userdata_name<T>().length(), userdata_name<T>().data() );
			};

			if constexpr ( detail::NewIndexable<T> )
			{
				using K = detail::default_key_type_t<T>;
				using V = detail::default_value_type_t<T>;

				bool is_string = k.is<std::string_view>();
				if ( is_string )
				{
					bool success = find_field( k.as<std::string_view>(), field_indexer );
					if ( success ) return;
				}
				if ( std::is_same_v<K, const char*> || std::is_same_v<K, std::string_view> || std::is_same_v<K, std::string> ? is_string : k.is<K>() )
					u.value()[ k.as<K>() ] = v.as<V>();
				else
					detail::type_error( L, k.slot(), "valid key" );
			}
			else
			{
				find_field_or_die( L, k.as<std::string_view>(), field_indexer );
			}
		}

		// String conversation of the object.
		//
		static std::string tostring( const userdata_wrapper<T>& u )
		{
			if constexpr ( detail::HasToString<T> )
				return std::string{ u.get()->to_string() };
			else
				return std::string{ userdata_name<T>() };
		}

		// Comparison of the object.
		//
		static bool eq( const userdata_wrapper<T>& a, T* b )
		{
			if ( a.get() == b ) return true;

			if constexpr ( detail::EqComparable<T> )
				return a.value() == *b;
			
			return false;
		}
		static bool lt( const userdata_wrapper<T>& a, T* b )
		{
			if ( a.get() == b ) return false;

			if constexpr ( detail::LtComparable<T> )
				return a.value() < *b;
			else if constexpr ( detail::GeComparable<T> )
				return !( a.value() >= *b );
			else if constexpr ( detail::LeComparable<T> && detail::EqComparable<T> )
				return a.value() != *b && a.value() <= *b;

			return a.get() < b;
		}
		static bool le( const userdata_wrapper<T>& a, T* b )
		{
			if ( a.get() == b ) return true;

			if constexpr ( detail::LeComparable<T> )
				return a.value() <= *b;
			else if constexpr ( detail::GtComparable<T> )
				return !( a.value() > *b );
			else if constexpr ( detail::LtComparable<T> && detail::EqComparable<T> )
				return a.value() == *b || a.value() < *b;
			
			return a.get() < b;
		}
		
		// Garbage collection of the object.
		//
		static void gc( const userdata_wrapper<T>& u ) { u.destroy(); }

		// Sets up the metatable for the first time.
		//
		ULUA_COLD static void setup( lua_State* L, stack::slot i )
		{
			// Set all the always existing properties.
			//
			stack_table metatable{ L, i, weak_t{} };
			metatable[ meta::metatable ] = 0;
			metatable[ meta::newindex ] = detail::constant<&newindex>();
			metatable[ meta::index ] = detail::constant<&index>();
			metatable[ meta::gc ] = detail::constant<&gc>();
			metatable[ meta::tostring ] = detail::constant<&tostring>();
			metatable[ meta::eq ] = detail::constant<&eq>();
			metatable[ meta::lt ] = detail::constant<&lt>();
			metatable[ meta::le ] = detail::constant<&le>();
			metatable[ meta::name ] = userdata_name<T>();

			// If the object has a length/size getters or is iterable, define the function.
			//
			if constexpr ( detail::HasLength<T> || detail::HasSize<T> || detail::Iterable<T> )
			{
				metatable[ meta::len ] = [ ] ( const userdata_wrapper<T>& a )
				{
					if constexpr ( detail::HasLength<T> )
						return a.get()->length();
					if constexpr ( detail::HasSize<T> )
						return std::size( a.value() );
					else
						return std::end( a.value() ) - std::begin( a.value() );
				};
			}

			// If the object is key/value iterable, define the function.
			//
			if constexpr ( detail::KvIterable<T> )
			{
				metatable[ meta::pairs ]  = [ ] ( T* a )
				{
					return std::make_tuple(
						[ it = std::begin( *a ) ] ( lua_State* L, const userdata_wrapper<T>& a, stack_reference ) mutable
						{
							if ( it != std::end( a.value() ) )
							{
								stack::push( L, it->first );
								stack::push( L, it->second );
								++it;
								return push_count{ 2 };
							}
							else
							{
								return push_count{ 0 };
							}
						},
						a,
						nil
					);
				};
			}
			// If the object is index iterable, define the function.
			//
			else if constexpr ( detail::Iterable<T> )
			{
				metatable[ meta::ipairs ] = [ ] ( T* a )
				{
					return std::make_tuple(
						[ it = std::begin( *a ) ] ( lua_State* L, const userdata_wrapper<T>& a, int key ) mutable
						{
							if ( it != std::end( a.value() ) )
							{
								stack::push( L, key + 1 );
								stack::push( L, *it );
								++it;
								return push_count{ 2 };
							}
							else
							{
								return push_count{ 0 };
							}
						},
						a,
						-1
					);
				};
				metatable[ meta::pairs ] = metatable[ meta::ipairs ];
			}

			// Define the arithmetic operators where possible.
			//
			if constexpr ( detail::Negable<T> )
			{
				metatable[ meta::unm ] = [ ] ( const userdata_wrapper<T>& a ) -> decltype( auto ) { return -a.value(); };
			}
			if constexpr ( detail::Addable<T, T> )
			{
				metatable[ meta::concat ] = [ ] ( const userdata_wrapper<T>& a, const userdata_wrapper<T>& b ) -> decltype( auto )
				{
					return a.value() + b.value();
				};
			}
			if constexpr ( detail::Addable<T, T> || detail::Addable<T, double> )
			{
				metatable[ meta::add ] = [ ] ( const userdata_wrapper<T>& a, stack_object obj ) -> decltype( auto )
				{
					if constexpr ( detail::Addable<T, T> )
						if ( obj.is<userdata_wrapper<T>>() )
							return a.value() + obj.as<userdata_wrapper<T>>().value();
					if constexpr ( detail::Addable<T, double> )
						if ( obj.is<double>() )
							return a.value() + obj.as<double>();

					std::string expected;
					if constexpr ( detail::Addable<T, T> && detail::Addable<T, double> )
						expected = std::string{ userdata_name<T>() } + " or number";
					else if constexpr ( detail::Addable<T, double> )
						expected = "number";
					else
						expected = std::string{ userdata_name<T>() };
					detail::type_error( obj.state(), obj.slot(), expected.data() );
				};
			}
			if constexpr ( detail::Subable<T, T> || detail::Subable<T, double> )
			{
				metatable[ meta::sub ] = [ ] ( const userdata_wrapper<T>& a, stack_object obj ) -> decltype( auto )
				{
					if constexpr ( detail::Subable<T, T> )
						if ( obj.is<userdata_wrapper<T>>() )
							return a.value() - obj.as<userdata_wrapper<T>>().value();
					if constexpr ( detail::Subable<T, double> )
						if ( obj.is<double>() )
							return a.value() - obj.as<double>();

					std::string expected;
					if constexpr ( detail::Subable<T, T> && detail::Subable<T, double> )
						expected = std::string{ userdata_name<T>() } + " or number";
					else if constexpr ( detail::Subable<T, double> )
						expected = "number";
					else
						expected = std::string{ userdata_name<T>() };
					detail::type_error( obj.state(), obj.slot(), expected.data() );
				};
			}
			if constexpr ( detail::Mulable<T, T> || detail::Mulable<T, double> )
			{
				metatable[ meta::mul ] = [ ] ( const userdata_wrapper<T>& a, stack_object obj ) -> decltype( auto )
				{
					if constexpr ( detail::Mulable<T, T> )
						if ( obj.is<userdata_wrapper<T>>() )
							return a.value() * obj.as<userdata_wrapper<T>>().value();
					if constexpr ( detail::Mulable<T, double> )
						if ( obj.is<double>() )
							return a.value() * obj.as<double>();

					std::string expected;
					if constexpr ( detail::Mulable<T, T> && detail::Mulable<T, double> )
						expected = std::string{ userdata_name<T>() } + " or number";
					else if constexpr ( detail::Mulable<T, double> )
						expected = "number";
					else
						expected = std::string{ userdata_name<T>() };
					detail::type_error( obj.state(), obj.slot(), expected.data() );
				};
			}
			if constexpr ( detail::Divable<T, T> || detail::Divable<T, double> )
			{
				metatable[ meta::div ] = [ ] ( const userdata_wrapper<T>& a, stack_object obj ) -> decltype( auto )
				{
					if constexpr ( detail::Divable<T, T> )
						if ( obj.is<userdata_wrapper<T>>() )
							return a.value() / obj.as<userdata_wrapper<T>>().value();
					if constexpr ( detail::Divable<T, double> )
						if ( obj.is<double>() )
							return a.value() / obj.as<double>();

					std::string expected;
					if constexpr ( detail::Divable<T, T> && detail::Divable<T, double> )
						expected = std::string{ userdata_name<T>() } + " or number";
					else if constexpr ( detail::Divable<T, double> )
						expected = "number";
					else
						expected = std::string{ userdata_name<T>() };
					detail::type_error( obj.state(), obj.slot(), expected.data() );
				};
			}
			if constexpr ( detail::Idivable<T, T> || detail::Idivable<T, int64_t> )
			{
				metatable[ meta::idiv ] = [ ] ( const userdata_wrapper<T>& a, stack_object obj ) -> size_t
				{
					if constexpr ( detail::Idivable<T, T> )
						if ( obj.is<userdata_wrapper<T>>() )
							return a.value() / obj.as<userdata_wrapper<T>>().value();
					if constexpr ( detail::Idivable<T, int64_t> )
						if ( obj.is<double>() )
							return a.value() / obj.as<int64_t>();

					std::string expected;
					if constexpr ( detail::Idivable<T, T> && detail::Idivable<T, double> )
						expected = std::string{ userdata_name<T>() } + " or number";
					else if constexpr ( detail::Idivable<T, double> )
						expected = "number";
					else
						expected = std::string{ userdata_name<T>() };
					detail::type_error( obj.state(), obj.slot(), expected.data() );
				};
			}
			if constexpr ( detail::Modable<T, T> || detail::Modable<T, int64_t> )
			{
				metatable[ meta::mod ] = [ ] ( const userdata_wrapper<T>& a, stack_object obj ) -> decltype( auto )
				{
					if constexpr ( detail::Modable<T, T> )
						if ( obj.is<userdata_wrapper<T>>() )
							return a.value() % obj.as<userdata_wrapper<T>>().value();
					if constexpr ( detail::Modable<T, int64_t> )
						if ( obj.is<double>() )
							return a.value() % obj.as<int64_t>();

					std::string expected;
					if constexpr ( detail::Modable<T, T> && detail::Modable<T, double> )
						expected = std::string{ userdata_name<T>() } + " or number";
					else if constexpr ( detail::Modable<T, double> )
						expected = "number";
					else
						expected = std::string{ userdata_name<T>() };
					detail::type_error( obj.state(), obj.slot(), expected.data() );
				};
			}
			if constexpr ( detail::Powable<T, T> || detail::Powable<T, int64_t> )
			{
				metatable[ meta::pow ] = [ ] ( const userdata_wrapper<T>& a, stack_object obj ) -> decltype( auto )
				{
					if constexpr ( detail::Powable<T, T> )
						if ( obj.is<userdata_wrapper<T>>() )
							return pow( a.value(), obj.as<userdata_wrapper<T>>().value() );
					if constexpr ( detail::Powable<T, int64_t> )
						if ( obj.is<double>() )
							return pow( a.value(), obj.as<int64_t>() );

					std::string expected;
					if constexpr ( detail::Powable<T, T> && detail::Powable<T, double> )
						expected = std::string{ userdata_name<T>() } + " or number";
					else if constexpr ( detail::Powable<T, double> )
						expected = "number";
					else
						expected = std::string{ userdata_name<T>() };
					detail::type_error( obj.state(), obj.slot(), expected.data() );
				};
			}
		}

		// Pushes the metatable on stack.
		//
		inline static void push( lua_State* L )
		{
			if ( stack::create_metatable( L, userdata_mt_name<T>().data() ) )
				setup( L, stack::top_t{} );
		}

		// Gets the metatable.
		//
		template<typename R = table>
		inline static R get( lua_State* L )
		{
			push( L );
			return R{ L, stack::top_t{} };
		}

		// Indexes the metatable.
		//
		inline static auto at( lua_State* L, meta key )
		{
			push( L );
			return detail::table_proxy<meta, false>{ L, stack::top( L ), true, std::move( key ) };
		}
	};
};