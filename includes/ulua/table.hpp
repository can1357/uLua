#pragma once
#include "common.hpp"
#include "reference.hpp"
#include "lazy.hpp"

namespace ulua
{
	namespace detail
	{
		// Table proxy.
		//
		template<TableKey Key, bool Raw>
		struct table_proxy : lazy<table_proxy<Key, Raw>>
		{
			static constexpr bool is_direct = false;

			stack_reference table = {};
			Key key = {};
			explicit inline table_proxy( lua_State* L, stack::slot slot, bool owning, Key key ) : table( L, slot ), key( key ) { table.ownership_flag = owning; }

			// Reference-like properties.
			//
			inline void push() const
			{
				stack::get_field( table.state(), table.slot(), key, std::bool_constant<Raw>{} );
			}
			inline lua_State* state() const { return table.state(); }

			// Setter.
			//
			template<typename T>
			inline void set( T&& value )
			{
				stack::push<T>( table.state(), std::forward<T>( value ) );
				stack::set_field( table.state(), table.slot(), key, std::bool_constant<Raw>{} );
			}
			template<typename T> inline table_proxy& operator=( T&& value ) { set<T>( std::forward<T>( value ) ); return *this; }
		};
		template<bool Raw, TableKey Key> static table_proxy<Key, Raw> make_table_proxy( lua_State* L, stack::slot slot, bool owning, Key key ) { return table_proxy<Key, Raw>{ L, slot, owning, key }; }
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

	// Table.
	//
	template<Reference Ref>
	struct basic_table : Ref, detail::lazy<basic_table<Ref>>
	{
		// TODO: Checks?

		inline basic_table() {}
		template<typename... Tx>
		explicit inline basic_table( Tx&&... ref ) : Ref( std::forward<Tx>( ref )... ) {}

		// TODO: Length
		//
		inline iterator begin() const { return stack_reference{ *this }; }
		inline iterator end() const { return {}; }
	};
	using table =       basic_table<registry_reference>;
	using stack_table = basic_table<stack_reference>;
};