#pragma once
#include "common.hpp"
#include "reference.hpp"
#include "lazy.hpp"

namespace ulua
{
	namespace detail
	{
		struct new_table_tag_t {};

		// Table proxy.
		//
		template<typename Key, bool Raw>
		struct table_proxy : lazy<table_proxy<Key, Raw>>
		{
			static constexpr bool is_direct = false;

			stack_reference table = {};
			Key key;
			explicit inline table_proxy( lua_State* L, stack::slot slot, bool owning, Key&& key ) : table( L, slot ), key( std::forward<Key>( key ) ) { table.ownership_flag = owning; }

			// Reference-like properties.
			//
			inline void push() const
			{
				stack::get_field( table.state(), table.slot(), key, std::bool_constant<Raw>{} );
			}
			inline lua_State* state() const { return table.state(); }
			inline void reset() { table.reset(); }
			inline void reset( detail::unchecked_t tag ) { table.reset( tag ); }

			// Setter.
			//
			template<typename T>
			inline void set( T&& value )
			{
				stack::push( table.state(), std::forward<T>( value ) );
				stack::set_field( table.state(), table.slot(), key, std::bool_constant<Raw>{} );
			}
			template<typename T = new_table_tag_t>
			inline table_proxy& operator=( T&& value ) 
			{ 
				if constexpr ( std::is_same_v<std::decay_t<T>, new_table_tag_t> )
				{
					stack::create_table( table.state() );
					stack::set_field( table.state(), table.slot(), key, std::bool_constant<Raw>{} );
					return *this;
				}
				else
				{
					set<T>( std::forward<T>( value ) );
					return *this;
				}
			}
		};
		template<bool Raw, typename Key>
		static table_proxy<Key, Raw> make_table_proxy( lua_State* L, stack::slot slot, bool owning, Key&& key ) { return table_proxy<Key, Raw>{ L, slot, owning, std::forward<Key>( key ) }; }
	};
	
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

	// Create tag.
	//
	struct create : reserve_table { inline constexpr create( reserve_table rsvd = {} ) : reserve_table( rsvd ) {} };

	// Table.
	//
	template<Reference Ref>
	struct basic_table : Ref, detail::lazy_indexable<basic_table<Ref>>
	{
		inline static bool check( lua_State* L, int& slot ) 
		{
#if ULUA_ACCEL
			bool res = tvistab( accel::ref( L, slot++ ) );
#else
			bool res = stack::type( L, slot++ ) == value_type::table;
#endif
			return res; 
		}
		inline static void check_asserted( lua_State* L, int slot )
		{
			if ( !check( L, slot ) )
				type_error( L, slot - 1, "table" );
		}

		inline constexpr basic_table() {}
		template<typename... Tx> requires( sizeof...( Tx ) != 0 && detail::Constructible<Ref, Tx...> )
		explicit inline constexpr basic_table( Tx&&... ref ) : Ref( std::forward<Tx>( ref )... ) {}

		// Creating constructor.
		//
		inline basic_table( lua_State* L, create tag )
		{
			stack::create_table( L, tag );
			Ref ref{ L, stack::top_t{} };
			Ref::swap( ref );
		}

		inline iterator begin() const { return stack_reference{ *this }; }
		inline iterator end() const { return {}; }
	};
	using table =       basic_table<registry_reference>;
	using stack_table = basic_table<stack_reference>;

	// Convenience helper to freeze tables.
	//
	template<typename Ref>
	inline void freeze_table( const basic_table<Ref>& table ) 
	{
		table.push();
		if ( !stack::push_metatable( table.state(), stack::top_t{} ) )
			stack::create_table( table.state(), ulua::reserve_records{ 1 } );
		stack::push( table.state(), [ ] ( lua_State* L ) { ulua::error( L, "cannot modify immutable table." ); } );
		stack::set_field( table.state(), -2, ulua::meta::newindex );
		stack::set_metatable( table.state(), -2 );
		stack::pop_n( table.state(), 1 );
	}
};