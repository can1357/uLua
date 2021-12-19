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
		using default_key_type_t = std::conditional_t<std::is_same_v<typename default_key_type<T>::type, std::string>, const char*, typename default_key_type<T>::type>;

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
		template<typename T> concept FindIndexable = requires( const T& v ) { v.find( std::declval<default_key_type_t<T>>() )->second; };
		template<typename T> concept Indexable = requires( const T& v ) { v[ std::declval<default_key_type_t<T>>() ]; };
		template<typename T> concept NewIndexable = requires( T& v ) { v[ std::declval<default_key_type_t<T>>() ] = std::declval<default_value_type_t<T>>(); };
		template<typename T> concept Negable = requires( const T& v ) { -v; };
		template<typename T, typename O> concept Addable = requires( const T& v, const O& v2 ) { v + v2; };
		template<typename T, typename O> concept Subable = requires( const T& v, const O& v2 ) { v - v2; };
		template<typename T, typename O> concept Mulable = requires( const T& v, const O& v2 ) { v * v2; };
		template<typename T, typename O> concept Divable = requires( const T& v, const O& v2 ) { v / v2; };
		template<typename T, typename O> concept Idivable = requires( const T& v, const O& v2 ) { size_t( v / v2 ); };
		template<typename T, typename O> concept Modable = requires( const T& v, const O& v2 ) { v% v2; };
		template<typename T, typename O> concept Powable = requires( const T & v, const O & v2 ) { pow( v, v2 ); };
		template<typename T, typename O> concept Xorable = requires( const T& v, const O& v2 ) { v ^ v2; };
	};

	// Readonly tag.
	//
	struct readonly_t {};

	// Helpers for the user to expose members or properties.
	//
	template<typename T>
	struct metaproperty_descriptor
	{
		T value;
		meta field;
		inline constexpr metaproperty_descriptor( meta field, T&& value ) : value( std::forward<T>( value ) ), field( field ) {}
	};
	template<auto Value> static constexpr auto property( meta field ) { return metaproperty_descriptor{ field, constant<Value>() }; }
	template<typename T> static constexpr auto property( meta field, T&& value ) { return metaproperty_descriptor<T>{ field, std::forward<T>( value ) }; }

	template<typename G, typename S>
	struct member_descriptor
	{
		G getter;
		S setter;
		std::string_view name;
		inline constexpr member_descriptor( std::string_view name, G&& getter, S&& setter ) : getter( std::forward<G>( getter ) ), setter( std::forward<S>( setter ) ), name( name ) {}

		template<typename T>
		inline void get( lua_State* L, stack::slot key_slot, T* value ) const
		{
			if constexpr ( detail::Callable<G, lua_State*, T*> )
				stack::push( L, getter( L, value ) );
			else
				error( L, "attempt to get write-only field '%.*s'", name.length(), name.data() );
		}

		template<typename T>
		inline void set( lua_State* L, stack::slot key_slot, T* value, const stack_object& ref ) const
		{
			if constexpr ( detail::Callable<S, lua_State*, T*, const stack_object&> )
				setter( L, value, ref );
			else
				error( L, "attempt to set read-only field '%.*s'", name.length(), name.data() );
		}
	};
	template<typename G, typename S> member_descriptor( std::string_view, G&&, S&& )->member_descriptor<G, S>;

	template<auto Field>
	static constexpr auto member( std::string_view name )
	{
		if constexpr ( detail::is_member_function_v<decltype( Field )> )
		{
			return member_descriptor{
				name,
				[ ] ( lua_State*, auto* ) { return constant<Field>(); },
				std::nullopt
			};
		}
		else if constexpr ( detail::is_member_field_v<decltype( Field )> )
		{
			return member_descriptor{
				name,
				[ ] ( lua_State*, auto* p ) -> decltype( auto ) { return p->*Field; },
				[ ] ( lua_State*, auto* p, const stack_object& value ) { p->*Field = (std::decay_t<decltype( p->*Field )>) value; }
			};
		}
		else
		{
			static_assert( sizeof( Field ) == -1, "Invalid constant member type." );
		}
	}
	template<typename T>
	static constexpr auto static_member( std::string_view name, T&& value ) 
	{
		using V = std::decay_t<T>;
		if constexpr ( std::is_empty_v<V> )
		{
			return member_descriptor{
				name,
				[ ] ( lua_State*, auto* ) { return V{}; },
				std::nullopt
			};
		}
		else
		{
			return member_descriptor{
				name,
				[ v = std::forward<T>( value ) ] ( lua_State*, auto* ) { return v; },
				std::nullopt
			};
		}
	}
	template<auto Field>
	static constexpr auto member( std::string_view name, readonly_t )
	{
		return member_descriptor{
			name,
			[ ] ( lua_State*, auto* p ) -> decltype( auto ) { return p->*Field; },
			std::nullopt
		};
	}

	namespace impl
	{
		template<typename G>
		inline constexpr auto make_getter( G&& g )
		{
			if constexpr ( std::is_member_function_pointer_v<std::decay_t<G>> )
			{
				if constexpr ( std::tuple_size_v<typename detail::function_traits<std::decay_t<G>>::arguments> != 0 )
					return [ g = std::forward<G>( g ) ]( lua_State* L, auto* p ) -> decltype( auto ) { return ( p->*g )( L ); };
				else
					return [ g = std::forward<G>( g ) ]( lua_State*, auto* p ) -> decltype( auto ) { return ( p->*g )(); };
			}
			else
			{
				if constexpr ( std::tuple_size_v<typename detail::function_traits<std::decay_t<G>>::arguments> != 1 )
					return [ g = std::forward<G>( g ) ]( lua_State* L, auto* p ) -> decltype( auto ) { return g( L, *p ); };
				else
					return [ g = std::forward<G>( g ) ]( lua_State*, auto* p ) -> decltype( auto ) { return g( *p ); };
			}
		}
		template<typename S>
		inline constexpr auto make_setter( S&& s )
		{
			if constexpr ( std::is_member_function_pointer_v<std::decay_t<S>> )
			{
				if constexpr ( std::tuple_size_v<typename detail::function_traits<std::decay_t<S>>::arguments> != 1 )
					return [ s = std::forward<S>( s ) ]( lua_State* L, auto* p, const stack_object& value ) -> decltype( auto ) { return ( p->*s )( L, value ); };
				else
					return [ s = std::forward<S>( s ) ]( lua_State*, auto* p, const stack_object& value ) -> decltype( auto ) { return ( p->*s )( value ); };
			}
			else
			{
				if constexpr ( std::tuple_size_v<typename detail::function_traits<std::decay_t<S>>::arguments> != 2 )
					return [ s = std::forward<S>( s ) ]( lua_State* L, auto* p, const stack_object& value ) -> decltype( auto ) { return s( L, *p, value ); };
				else
					return [ s = std::forward<S>( s ) ]( lua_State*, auto* p, const stack_object& value ) -> decltype( auto ) { return s( *p, value ); };
			}
		}
	};

	template<typename G>
	static constexpr auto property( std::string_view name, G&& getter )
	{
		return member_descriptor{ name, impl::make_getter<G>( std::forward<G>( getter ) ), std::nullopt };
	}
	template<typename G, typename S>
	static constexpr auto property( std::string_view name, G&& getter, S&& setter )
	{
		return member_descriptor{ name, impl::make_getter<G>( std::forward<G>( getter ) ), impl::make_setter<S>( std::forward<S>( setter ) ) };
	}

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
				return detail::visit_index<max_field_length + 1>( i, [ & ] <size_t N> ( const_tag<N> ) FORCE_INLINE
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

		// Implement a metatable finding helper.
		//
		template<meta M, typename F>
		static constexpr bool find_meta( F&& fn )
		{
			bool found = false;
			detail::enum_tuple( userdata_meta<T>, [ & ] ( const auto& value )
			{
				if ( !found && value.field == M )
				{
					fn( value.value );
					found = true;
				}
			} );
			return found;
		}
		template<meta M> static constexpr bool has_meta() { return find_meta<M>( [ & ] ( const auto& ) {} ); }
		template<meta M> static inline bool set_meta( stack_table& tbl ) { return find_meta<M>( [ & ] ( const auto& value ) { tbl[ M ] = value; } ); }

		// Indexing of the object.
		//
		static push_count index( lua_State* L, const userdata_wrapper<const T>& u, const stack_object& k )
		{
			auto field_indexer = [ & ] ( auto& field ) { field.get( L, k.slot(), u.get() ); };
			if ( k.is<std::string_view>() && find_field( k.as<std::string_view>(), field_indexer ) )
				return { 1 };

			if constexpr ( detail::Indexable<T> )
			{
				using K = detail::default_key_type_t<T>;
				if ( k.is<K>() )
				{
					stack::push( L, u.value()[ k.as<K>() ] );
					return { 1 };
				}
			}
			else if constexpr ( detail::FindIndexable<T> )
			{
				using K = detail::default_key_type_t<T>;
				if ( k.is<K>() )
				{
					const T& ref = u.value();
					if ( auto it = ref.find( k.as<K>() ); it != ref.end() )
					{
						stack::push( L, it->second );
						return { 1 };
					}
				}
			}

			error( L, "attempt to get undefined field '%s'", stack::to_string( L, k.slot() ).data() );
		}
		static void newindex( lua_State* L, const userdata_wrapper<T>& u, const stack_object& k, const stack_object& v )
		{
			auto field_indexer = [ & ] ( auto& field ) { field.set( L, k.slot(), u.get(), v ); };
			if ( k.is<std::string_view>() && find_field( k.as<std::string_view>(), field_indexer ) )
				return;

			if constexpr ( detail::NewIndexable<T> )
			{
				using K = detail::default_key_type_t<T>;
				using V = detail::default_value_type_t<T>;

				if ( k.is<K>() )
				{
					u.value()[ k.as<K>() ] = v.as<V>();
					return;
				}
			}

			error( L, "attempt to set undefined field '%s'", stack::to_string( L, k.slot() ).data() );
		}

		// String conversation of the object.
		//
		static std::string tostring( const userdata_wrapper<const T>& u )
		{
			if constexpr ( detail::HasToString<T> )
				return std::string{ u.get()->to_string() };
			else
				return std::string{ userdata_name<T>() };
		}

		// Comparison of the object.
		//
		static bool eq( const userdata_wrapper<const T>& a, const T* b )
		{
			if ( a.get() == b ) return true;

			if constexpr ( detail::EqComparable<T> )
				return a.value() == *b;
			
			return false;
		}
		static bool lt( const userdata_wrapper<const T>& a, const T* b )
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
		static bool le( const userdata_wrapper<const T>& a, const T* b )
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
		static void gc( const userdata_wrapper<const T>& u ) { u.destroy(); }

		// Sets up the metatable for the first time.
		//
		ULUA_COLD static void setup( lua_State* L, stack::slot i )
		{
			// Set all the always existing properties.
			//
			stack_table metatable{ L, i, weak_t{} };
			if ( !set_meta<meta::metatable>( metatable ) ) metatable[ meta::metatable ] = 0;
			if ( !set_meta<meta::newindex>( metatable ) )  metatable[ meta::newindex ] = constant<&newindex>();
			if ( !set_meta<meta::index>( metatable ) )     metatable[ meta::index ] = constant<&index>();
			if ( !set_meta<meta::tostring>( metatable ) )  metatable[ meta::tostring ] = constant<&tostring>();
			if ( !set_meta<meta::eq>( metatable ) )        metatable[ meta::eq ] = constant<&eq>();
			if ( !set_meta<meta::lt>( metatable ) )        metatable[ meta::lt ] = constant<&lt>();
			if ( !set_meta<meta::le>( metatable ) )        metatable[ meta::le ] = constant<&le>();
			if ( !set_meta<meta::name>( metatable ) )      metatable[ meta::name ] = userdata_name<T>();
			if ( !set_meta<meta::gc>( metatable ) && !std::is_trivially_destructible_v<T> )        
				metatable[ meta::gc ] = constant<&gc>();
			
			// Propagate call property.
			//
			if constexpr ( has_meta<meta::call>() )
				set_meta<meta::call>( metatable );

			// If the object has a length/size getters or is iterable, define the function.
			//
			if constexpr ( has_meta<meta::len>() )
			{
				set_meta<meta::len>( metatable );
			}
			else if constexpr ( detail::HasLength<T> || detail::HasSize<T> || detail::Iterable<T> )
			{
				metatable[ meta::len ] = [ ] ( const userdata_wrapper<const T>& a )
				{
					if constexpr ( detail::HasLength<T> )
						return a.get()->length();
					else if constexpr ( detail::HasSize<T> )
						return std::size( a.value() );
					else
						return std::end( a.value() ) - std::begin( a.value() );
				};
			}

			// Set user-defined pairs/ipairs.
			//
			if constexpr ( has_meta<meta::pairs>() )
			{
				set_meta<meta::pairs>( metatable );
				set_meta<meta::ipairs>( metatable );
			}
			else
			{
				// If the object is key/value iterable, define the function.
				//
				if constexpr ( detail::KvIterable<T> )
				{
					metatable[ meta::pairs ]  = [ ] ( const T* a )
					{
						return std::make_tuple(
							[ it = std::begin( *a ) ] ( lua_State* L, const userdata_wrapper<const T>& a, stack_reference ) mutable
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
					metatable[ meta::ipairs ] = [ ] ( const T* a )
					{
						return std::make_tuple(
							[ it = std::begin( *a ) ] ( lua_State* L, const userdata_wrapper<const T>& a, int key ) mutable
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
			}

			// Define the arithmetic operators where possible.
			//
			if constexpr ( has_meta<meta::unm>() )
			{
				set_meta<meta::unm>( metatable );
			}
			else if constexpr ( detail::Negable<T> )
			{
				metatable[ meta::unm ] = [ ] ( const userdata_wrapper<const T>& a ) -> decltype( auto ) { return -a.value(); };
			}
			if constexpr ( has_meta<meta::concat>() )
			{
				set_meta<meta::concat>( metatable );
			}
			else if constexpr ( detail::Addable<T, T> )
			{
				metatable[ meta::concat ] = [ ] ( const userdata_wrapper<const T>& a, const userdata_wrapper<const T>& b ) -> decltype( auto )
				{
					return a.value() + b.value();
				};
			}

			if constexpr ( has_meta<meta::add>() )
			{
				set_meta<meta::add>( metatable );
			}
			else if constexpr ( detail::Addable<T, T> || detail::Addable<T, double> || detail::Addable<double, T> )
			{
				metatable[ meta::add ] = [ ] ( const stack_object& a, const stack_object& b ) -> decltype( auto )
				{
					bool a_fp = a.is<double>();
					bool a_uv = !a_fp && a.is<userdata_wrapper<const T>>();
					bool b_fp = b.is<double>();
					bool b_uv = !b_fp && b.is<userdata_wrapper<const T>>();

					if constexpr ( detail::Addable<T, T> )
						if ( a_uv && b_uv )
							return a.as<userdata_wrapper<const T>>().value() + b.as<userdata_wrapper<const T>>().value();
					if constexpr ( detail::Addable<T, double> )
						if ( a_uv && b_fp )
							return a.as<userdata_wrapper<const T>>().value() + b.as<double>();
					if constexpr ( detail::Addable<double, T> )
						if ( a_fp && b_uv )
							return a.as<double>() + b.as<userdata_wrapper<const T>>().value();

					if ( a_uv ) 
						type_error( b.state(), b.slot(), b_uv ? "number" : userdata_name<T>().data() );
					else
						type_error( a.state(), a.slot(), a_uv ? "number" : userdata_name<T>().data() );
				};
			}
			
			if constexpr ( has_meta<meta::sub>() )
			{
				set_meta<meta::sub>( metatable );
			}
			else if constexpr ( detail::Subable<T, T> || detail::Subable<T, double> )
			{
				metatable[ meta::sub ] = [ ] ( const userdata_wrapper<const T>& a, const stack_object& obj ) -> decltype( auto )
				{
					if constexpr ( detail::Subable<T, T> )
						if ( obj.is<userdata_wrapper<const T>>() )
							return a.value() - obj.as<userdata_wrapper<const T>>().value();
					if constexpr ( detail::Subable<T, double> )
						if ( obj.is<double>() )
							return a.value() - obj.as<double>();

					if constexpr ( detail::Subable<T, T> && detail::Subable<T, double> )
						type_error( obj.state(), obj.slot(), "%s or number", userdata_name<T>().data() );
					else if constexpr ( detail::Subable<T, double> )
						type_error( obj.state(), obj.slot(), "number" );
					else
						type_error( obj.state(), obj.slot(), userdata_name<T>().data() );
				};
			}

			if constexpr ( has_meta<meta::mul>() )
			{
				set_meta<meta::mul>( metatable );
			}
			else if constexpr ( detail::Mulable<T, T> || detail::Mulable<T, double> || detail::Mulable<double, T> )
			{
				metatable[ meta::mul ] = [ ] ( const stack_object& a, const stack_object& b ) -> decltype( auto )
				{
					bool a_fp = a.is<double>();
					bool a_uv = !a_fp && a.is<userdata_wrapper<const T>>();
					bool b_fp = b.is<double>();
					bool b_uv = !b_fp && b.is<userdata_wrapper<const T>>();

					if constexpr ( detail::Mulable<T, T> )
						if ( a_uv && b_uv )
							return a.as<userdata_wrapper<const T>>().value() * b.as<userdata_wrapper<const T>>().value();
					if constexpr ( detail::Mulable<T, double> )
						if ( a_uv && b_fp )
							return a.as<userdata_wrapper<const T>>().value() * b.as<double>();
					if constexpr ( detail::Mulable<double, T> )
						if ( a_fp && b_uv )
							return a.as<double>() * b.as<userdata_wrapper<const T>>().value();

					if ( a_uv ) 
						type_error( b.state(), b.slot(), b_uv ? "number" : userdata_name<T>().data() );
					else
						type_error( a.state(), a.slot(), a_uv ? "number" : userdata_name<T>().data() );
				};
			}

			if constexpr ( has_meta<meta::div>() )
			{
				set_meta<meta::div>( metatable );
			}
			else if constexpr ( detail::Divable<T, T> || detail::Divable<T, double> )
			{
				metatable[ meta::div ] = [ ] ( const userdata_wrapper<const T>& a, const stack_object& obj ) -> decltype( auto )
				{
					if constexpr ( detail::Divable<T, T> )
						if ( obj.is<userdata_wrapper<const T>>() )
							return a.value() / obj.as<userdata_wrapper<const T>>().value();
					if constexpr ( detail::Divable<T, double> )
						if ( obj.is<double>() )
							return a.value() / obj.as<double>();

					if constexpr ( detail::Divable<T, T> && detail::Divable<T, double> )
						type_error( obj.state(), obj.slot(), "%s or number", userdata_name<T>().data() );
					else if constexpr ( detail::Divable<T, double> )
						type_error( obj.state(), obj.slot(), "number" );
					else
						type_error( obj.state(), obj.slot(), userdata_name<T>().data() );
				};
			}

			if constexpr ( has_meta<meta::idiv>() )
			{
				set_meta<meta::idiv>( metatable );
			}
			else if constexpr ( detail::Idivable<T, T> || detail::Idivable<T, int64_t> )
			{
				metatable[ meta::idiv ] = [ ] ( const userdata_wrapper<const T>& a, const stack_object& obj ) -> size_t
				{
					if constexpr ( detail::Idivable<T, T> )
						if ( obj.is<userdata_wrapper<const T>>() )
							return a.value() / obj.as<userdata_wrapper<const T>>().value();
					if constexpr ( detail::Idivable<T, int64_t> )
						if ( obj.is<double>() )
							return a.value() / obj.as<int64_t>();

					if constexpr ( detail::Idivable<T, T> && detail::Idivable<T, double> )
						type_error( obj.state(), obj.slot(), "%s or number", userdata_name<T>().data() );
					else if constexpr ( detail::Idivable<T, double> )
						type_error( obj.state(), obj.slot(), "number" );
					else
						type_error( obj.state(), obj.slot(), userdata_name<T>().data() );
				};
			}
			if constexpr ( has_meta<meta::mod>() )
			{
				set_meta<meta::mod>( metatable );
			}
			else if constexpr ( detail::Modable<T, T> || detail::Modable<T, int64_t> )
			{
				metatable[ meta::mod ] = [ ] ( const userdata_wrapper<const T>& a, const stack_object& obj ) -> decltype( auto )
				{
					if constexpr ( detail::Modable<T, T> )
						if ( obj.is<userdata_wrapper<const T>>() )
							return a.value() % obj.as<userdata_wrapper<const T>>().value();
					if constexpr ( detail::Modable<T, int64_t> )
						if ( obj.is<double>() )
							return a.value() % obj.as<int64_t>();

					if constexpr ( detail::Modable<T, T> && detail::Modable<T, double> )
						type_error( obj.state(), obj.slot(), "%s or number", userdata_name<T>().data() );
					else if constexpr ( detail::Modable<T, double> )
						type_error( obj.state(), obj.slot(), "number" );
					else
						type_error( obj.state(), obj.slot(), userdata_name<T>().data() );
				};
			}
			if constexpr ( has_meta<meta::pow>() )
			{
				set_meta<meta::pow>( metatable );
			}
			else if constexpr ( detail::Powable<T, T> || detail::Powable<T, int64_t> )
			{
				metatable[ meta::pow ] = [ ] ( const userdata_wrapper<const T>& a, const stack_object& obj ) -> decltype( auto )
				{
					if constexpr ( detail::Powable<T, T> )
						if ( obj.is<userdata_wrapper<const T>>() )
							return pow( a.value(), obj.as<userdata_wrapper<const T>>().value() );
					if constexpr ( detail::Powable<T, int64_t> )
						if ( obj.is<double>() )
							return pow( a.value(), obj.as<int64_t>() );

					if constexpr ( detail::Powable<T, T> && detail::Powable<T, double> )
						type_error( obj.state(), obj.slot(), "%s or number", userdata_name<T>().data() );
					else if constexpr ( detail::Powable<T, double> )
						type_error( obj.state(), obj.slot(), "number" );
					else
						type_error( obj.state(), obj.slot(), userdata_name<T>().data() );
				};
			}
			else if constexpr ( detail::Xorable<T, T> || detail::Xorable<T, int64_t> )
			{
				metatable[ meta::pow ] = [ ] ( const userdata_wrapper<const T>& a, const stack_object& obj ) -> decltype( auto )
				{
					if constexpr ( detail::Xorable<T, T> )
						if ( obj.is<userdata_wrapper<const T>>() )
							return a.value() ^ obj.as<userdata_wrapper<const T>>().value();
					if constexpr ( detail::Xorable<T, int64_t> )
						if ( obj.is<double>() )
							return a.value() ^ obj.as<int64_t>();

					if constexpr ( detail::Xorable<T, T> && detail::Xorable<T, double> )
						type_error( obj.state(), obj.slot(), "%s or number", userdata_name<T>().data() );
					else if constexpr ( detail::Xorable<T, double> )
						type_error( obj.state(), obj.slot(), "number" );
					else
						type_error( obj.state(), obj.slot(), userdata_name<T>().data() );
				};
			}
		}

		// Pushes the metatable on stack.
		//
		inline static void push( lua_State* L )
		{
			if ( stack::create_metatable( L, userdata_mt_name<T>().data() ) ) [[unlikely]]
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