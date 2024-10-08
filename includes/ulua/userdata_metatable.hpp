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

		static void push_const_code( lua_State* L, const char* code )
		{
			lua_pushlightuserdata( L, ( void* ) &code[ 0 ] );
			lua_rawget( L, LUA_REGISTRYINDEX );
			if ( !stack::type_check<value_type::function>( L, stack::top_t{} ) ) [[unlikely]]
			{
				stack::pop_n( L, 1 );
				luaL_loadbuffer( L, code, strlen( code ), "internal" );
				lua_call( L, 0, 1 );
				lua_pushlightuserdata( L, ( void* ) &code[ 0 ] );
				stack::copy( L, -2 );
				lua_rawset( L, LUA_REGISTRYINDEX );
			}
		}
		template<typename... Tx>
		static void run_through( lua_State* L, const char* code, Tx&&... args )
		{
			push_const_code( L, code );
			lua_call( L, stack::push( L, std::forward_as_tuple( std::forward<Tx>( args )... ) ), 0 );
		}
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
	
	struct constant_getter_tag {};
	template<typename T>
	struct constant_getter : constant_getter_tag
	{
		T value;
		constexpr constant_getter( T&& value ) : value( std::forward<T>( value ) ) {}
	};
	template<typename T> constant_getter( T&& )->constant_getter<T>;

	struct bytecode_property
	{
		const char* code;
	};

	template<typename G, typename S>
	struct member_descriptor
	{
		G getter;
		S setter;
		const char* name;
		inline constexpr member_descriptor( const char* name, G&& getter, S&& setter ) : getter( std::forward<G>( getter ) ), setter( std::forward<S>( setter ) ), name( name ) {}

		inline void write_getter( stack_table& tbl ) const
		{
			if constexpr ( !std::is_same_v<G, nil_t> )
			{
				if constexpr ( std::is_base_of_v<constant_getter_tag, G> )
				{
					detail::run_through( tbl.state(), "return function(tbl, key, value) tbl[key] = function() return value end end", tbl, name, getter.value );
				}
				else if constexpr ( std::is_same_v<bytecode_property, G> )
				{
					detail::push_const_code( tbl.state(), getter.code );
					lua_setfield( tbl.state(), tbl.slot(), name );
				}
				else
				{
					tbl[ name ] = getter;
				}
			}
			else
			{
				detail::run_through( tbl.state(), "return function(tbl, key, x) tbl[key] = function() error(x, 2) end end", tbl, name, "getting write-only property" );
			}
		}
		inline void write_setter( stack_table& tbl ) const
		{
			if constexpr ( std::is_same_v<bytecode_property, S> )
			{
				detail::push_const_code( tbl.state(), setter.code );
				lua_setfield( tbl.state(), tbl.slot(), name );
			}
			else if constexpr ( !std::is_same_v<S, nil_t> )
			{
				tbl[ name ] = setter;
			}
			else
			{
				detail::run_through( tbl.state(), "return function(tbl, key, x) tbl[key] = function() error(x, 2) end end", tbl, name, "setting read-only property" );
			}
		}
	};
	template<typename G, typename S> member_descriptor( const char*, G&&, S&& )->member_descriptor<G, S>;

	template<typename T>
	static constexpr auto static_member( const char* name, T&& value )
	{
		return member_descriptor{
			name,
			constant_getter{ std::forward<T>( value ) },
			nil
		};
	}
	template<auto Field>
	static constexpr auto member( const char* name )
	{
		if constexpr ( detail::is_member_function_v<decltype( Field )> )
		{
			return member_descriptor{
				name,
				constant_getter{ constant<Field>() },
				nil
			};
		}
		else if constexpr ( detail::is_member_field_v<decltype( Field )> )
		{
			using T = detail::member_field_class_t<Field>;
			return member_descriptor{
				name,
				[ ] ( lua_State*, T* p ) -> decltype( auto ) { return p->*Field; },
				[ ] ( lua_State*, T* p, const stack_object& value ) { p->*Field = (std::decay_t<decltype( p->*Field )>) value; }
			};
		}
		else
		{
			static_assert( sizeof( Field ) == -1, "Invalid constant member type." );
		}
	}
	template<auto Field>
	static constexpr auto member( const char* name, readonly_t )
	{
		using T = detail::member_field_class_t<Field>;
		return member_descriptor{
			name,
			[ ] ( lua_State*, T* p ) -> decltype( auto ) { return p->*Field; },
			nil
		};
	}
	template<typename G>
	static constexpr auto property( const char* name, G&& getter )
	{
		return member_descriptor{ name, std::forward<G>( getter ), nil };
	}
	template<typename G, typename S>
	static constexpr auto property( const char* name, G&& getter, S&& setter )
	{
		return member_descriptor{ name, std::forward<G>( getter ), std::forward<S>( setter ) };
	}

	// Define the auto generated userdata metatable.
	//
	template<typename T>
	struct userdata_metatable
	{
		// Implement a metatable finding helper.
		//
		template<meta M, typename F>
		static constexpr bool find_meta( F&& fn )
		{
			return detail::find_tuple_if( userdata_meta<T>, [ & ] ( const auto& value )
			{
				if ( value.field != M )
					return false;
				fn( value.value );
				return true;
			} );
		}
		template<meta M> static constexpr bool has_meta() { return find_meta<M>( [ & ] ( const auto& ) {} ); }
		template<meta M> static inline bool set_meta( stack_table& tbl ) { return find_meta<M>( [ & ] ( const auto& value ) { tbl[ M ] = value; } ); }

		// Indexing of the object.
		//
		static push_count index( lua_State* L, const userdata_wrapper<const T>& u, const stack_object& k )
		{
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
			return { 0 };
		}
		static void newindex( lua_State* L, const userdata_wrapper<T>& u, const stack_object& k, const stack_object& v )
		{
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
			error( L, "setting undefined property" );
		}


		static void adjust_index( lua_State* L, stack_table& tbl )
		{
			stack::create_table( L );
			stack_table sindex{ L, stack::top_t{} };
			std::apply( [ & ] <typename... F> ( const F&... fields ) { ( fields.write_getter( sindex ), ... ); }, userdata_fields<T> );
			sindex.push();
			lua_setfield( tbl.state(), tbl.slot(), "__indexref" );
		}
		static void adjust_newindex( lua_State* L, stack_table& tbl )
		{
			stack::create_table( L );
			stack_table sindex{ L, stack::top_t{} };
			std::apply( [ & ] <typename... F> ( const F&... fields ) { ( fields.write_setter( sindex ), ... ); }, userdata_fields<T> );
			sindex.push();
			lua_setfield( tbl.state(), tbl.slot(), "__newindexref" );
		}

		// String conversation of the object.
		//
		static auto tostring( const userdata_wrapper<const T>& u )
		{
			if constexpr ( detail::HasToString<T> )
				return u.get()->to_string();
			else
				return std::string{ userdata_name<T>() };
		}

		// Comparison of the object.
		//
		static bool eq( userdata_value _a, const stack_object& _b )
		{
			if ( !_b.is<userdata_value>() ) return false;
			auto w1 = ( userdata_wrapper<const T>* ) _a.pointer;
			auto w2 = ( userdata_wrapper<const T>* ) _b.as<userdata_value>().pointer;
			if ( w1 == w2 ) return true;

			if ( !w1 || !w1->check_type() || !w2 || !w2->check_type() )
				return false;
			if ( w1->get() == w2->get() )
				return true;

			if constexpr ( detail::EqComparable<const T> ) {
				if ( !w1->check_life() || !w2->check_life() )
					return false;
				return w1->value() == w2->value();
			} else {
				return false;
			}
		}
		static bool lt( userdata_value _a, const stack_object& _b )
		{
			if ( !_b.is<userdata_value>() ) return false;
			auto w1 = ( userdata_wrapper<const T>* ) _a.pointer;
			auto w2 = ( userdata_wrapper<const T>* ) _b.as<userdata_value>().pointer;
			if ( w1 == w2 ) return false;

			bool w1ok = w1 && w1->check_type() && w1->check_life();
			bool w2ok = w2 && w2->check_type() && w2->check_life();
			if ( w1ok != w2ok ) return w1ok < w2ok;
			if ( w1->get() == w2->get() )
				return false;

			auto& a = w1->value();
			auto& b = w2->value();

			if constexpr ( detail::LtComparable<const T> )
				return a < b;
			else if constexpr ( detail::GeComparable<const T> )
				return !( a >= b );
			else if constexpr ( detail::LeComparable<const T> && detail::EqComparable<const T> )
				return a != b && a <= b;

			return uintptr_t( w1->get() ) <=uintptr_t( w2->get() );
		}
		static bool le( userdata_value _a, const stack_object& _b )
		{
			if ( !_b.is<userdata_value>() ) return false;
			auto w1 = ( userdata_wrapper<const T>* ) _a.pointer;
			auto w2 = ( userdata_wrapper<const T>* ) _b.as<userdata_value>().pointer;
			if ( w1 == w2 ) return true;

			bool w1ok = w1 && w1->check_type() && w1->check_life();
			bool w2ok = w2 && w2->check_type() && w2->check_life();
			if ( w1ok != w2ok ) return w1ok <= w2ok;
			if ( w1->get() == w2->get() )
				return true;

			auto& a = w1->value();
			auto& b = w2->value();

			if constexpr ( detail::LeComparable<const T> )
				return a <= b;
			else if constexpr ( detail::GtComparable<const T> )
				return !( a > b );
			else if constexpr ( detail::LtComparable<const T> && detail::EqComparable<const T> )
				return a == b || a < b;

			return uintptr_t( w1->get() ) <= uintptr_t( w2->get() );
		}
		
		// Garbage collection of the object.
		//
		static void gc( lua_State* L, userdata_value u ) {
			auto wrapper = ( userdata_wrapper<T>* ) u.pointer;
			if ( !wrapper || !wrapper->check_type() || !wrapper->check_life() )
				return;
			wrapper->destroy();
		}

		// Sets up the metatable for the first time.
		//
		ULUA_COLD static void setup( lua_State* L, stack::slot i )
		{
			// Set the constant properties.
			//
			stack_table metatable{ L, i, weak_t{} };
			set_meta<meta::call>( metatable );

			if ( !set_meta<meta::index>( metatable ) ) {
				if constexpr ( detail::Indexable<T> || detail::FindIndexable<T> )
					metatable[ meta::index ] = constant<&index>();
			}
			if ( !set_meta<meta::newindex>( metatable ) ) {
				if constexpr ( detail::NewIndexable<T> )
					metatable[ meta::newindex ] = constant<&newindex>();
			}
			adjust_index( L, metatable );
			adjust_newindex( L, metatable );

			detail::run_through( L, R"(
return function(table)
	do
		local dnindex = table.__newindex
		local snindex = table.__newindexref
		if type(dnindex) == "function" then
			table.__newindex = function(self, key, value)
				local field = snindex[key]
				if field then
					field(self, value)
				else
					dnindex(self, key, value)
				end
			end
		elseif dnindex then
			table.__newindex = function(self, key, value)
				local field = snindex[key]
				if field then
					field(self, value)
				else
					dnindex[key] = value
				end
			end
		else
			table.__newindex = function(self, key, value)
				local field = snindex[key]
				return field(self, value)
			end
		end
	end

	do
		local dindex = table.__index
		local sindex = table.__indexref
		if type(dindex) == "function" then
			table.__index = function(self, key)
				local field = sindex[key]
				if field then
					return field(self)
				else
					return dindex(self, key)
				end
			end
		elseif dindex then
			table.__index = function(self, key)
				local field = sindex[key]
				if field then
					return field(self)
				else
					return dindex[key]
				end
			end
		else
			table.__index = function(self, key)
				local field = sindex[key]
				return field and field(self)
			end
		end
	end
end
)", metatable );
			
			if ( !set_meta<meta::metatable>( metatable ) ) metatable[ meta::metatable ] = 0;
			if ( !set_meta<meta::tostring>( metatable ) )  metatable[ meta::tostring ] = constant<&tostring>();
			if ( !set_meta<meta::eq>( metatable ) )        metatable[ meta::eq ] = constant<&eq>();
			if ( !set_meta<meta::lt>( metatable ) )        metatable[ meta::lt ] = constant<&lt>();
			if ( !set_meta<meta::le>( metatable ) )        metatable[ meta::le ] = constant<&le>();
			if ( !set_meta<meta::name>( metatable ) )      metatable[ meta::name ] = userdata_name<T>();
			if ( !set_meta<meta::gc>( metatable ) && !std::is_trivially_destructible_v<T> )        
				metatable[ meta::gc ] = constant<&gc>();
			
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
			else if constexpr ( detail::Powable<T, T> || detail::Powable<T, double> )
			{
				metatable[ meta::pow ] = [ ] ( const userdata_wrapper<const T>& a, const stack_object& obj ) -> decltype( auto )
				{
					if constexpr ( detail::Powable<T, T> )
						if ( obj.is<userdata_wrapper<const T>>() )
							return pow( a.value(), obj.as<userdata_wrapper<const T>>().value() );
					if constexpr ( detail::Powable<T, double> )
						if ( obj.is<double>() )
							return pow( a.value(), obj.as<double>() );

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