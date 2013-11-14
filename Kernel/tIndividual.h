/* This file is part of the FaCT++ DL reasoner
Copyright (C) 2003-2013 by Dmitry Tsarkov

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef TINDIVIDUAL_H
#define TINDIVIDUAL_H

#include <map>

#include "tConcept.h"

class DlCompletionTree;
class TRelated;
class SaveLoadManager;

class TIndividual;

/// class to map roles to set of individuals
class TRelatedMap
{
public:		// interface types
		/// vector of individuals
	typedef std::vector<const TIndividual*> CIVec;

protected:	// types
		/// base class
	typedef std::map<const TRole*,CIVec> BaseType;

protected:	// members
		/// base that contains all info
	BaseType Base;

public:		// interface
		/// empty c'tor
	TRelatedMap ( void ) {}
		/// empty d'tor
	~TRelatedMap ( void ) {}

	// access

		/// check whether role is in map
	bool hasRole ( const TRole* R ) const { return Base.find(R) != Base.end(); }
		/// get array related to role
	const CIVec& getRelated ( const TRole* R ) const
	{
		fpp_assert ( hasRole(R) );
		return Base.find(R)->second;
	}
		/// add related wrt role
	void setRelated ( const TRole* R, const CIVec& v )
	{
		fpp_assert ( !hasRole(R) );
		Base[R] = v;
	}
}; // TRelatedMap

/// class to represent individuals
class TIndividual: public TConcept
{
public:		// types
		/// pointers to RELATED constructors
	typedef std::vector<TRelated*> RelatedSet;
		/// vector of individuals
	typedef TRelatedMap::CIVec CIVec;

public:		// members
		/// pointer to nominal node (works for singletons only)
	DlCompletionTree* node;

		/// index for axioms <this,C>:R
	RelatedSet RelatedIndex;
		/// map for the related individuals: Map[R]={i:R(this,i)}
	TRelatedMap* pRelatedMap;

private:	// no copy
		/// no copy c'tor
	TIndividual ( const TIndividual& );
		/// no asssignment
	TIndividual& operator = ( const TIndividual& );

public:		// interface
		/// the only c'tor
	explicit TIndividual ( const std::string& name )
		: TConcept(name)
		, node(NULL)
		, pRelatedMap(new TRelatedMap())
	{
		setSingleton(true);
	}
		/// empty d'tor
	virtual ~TIndividual ( void ) { delete pRelatedMap; }

		/// init told subsumers of the individual by it's description
	virtual void initToldSubsumers ( void )
	{
		toldSubsumers.clear();
		clearHasSP();
		if ( isRelated() )	// check if domain and range of RELATED axioms affects TS
			updateToldFromRelated();
		// normalise description if the only parent is TOP
		if ( isPrimitive() && Description && Description->Element() == TOP )
			removeDescription();

		// not a completely defined if there are extra rules or related individuals
		bool CD = !hasExtraRules() && isPrimitive() && !isRelated();
		if ( Description != NULL || hasToldSubsumers() )
			CD &= TConcept::initToldSubsumers(Description);
		setCompletelyDefined(CD);
	}

	// related things

		/// update told subsumers from the RELATED axioms in a given range
	template<class Iterator>
	void updateTold ( Iterator begin, Iterator end, RoleSSet& RolesProcessed )
	{
		for ( Iterator p = begin; p < end; ++p )
			SearchTSbyRoleAndSupers ( (*p)->getRole(), RolesProcessed);
	}
		/// update told subsumers from all relevant RELATED axioms
	void updateToldFromRelated ( void );
		/// check if individual connected to something with RELATED statement
	bool isRelated ( void ) const { return !RelatedIndex.empty(); }
		/// set individual related
	void addRelated ( TRelated* p ) { RelatedIndex.push_back(p); }
		/// add all the related elements from the given P
	void addRelated ( TIndividual* p )
		{ RelatedIndex.insert ( RelatedIndex.end(), p->RelatedIndex.begin(), p->RelatedIndex.end() ); }

	// related map access

		/// @return true if has cache for related individuals via role R
	bool hasRelatedCache ( const TRole* R ) const { return pRelatedMap->hasRole(R); }
		/// get set of individuals related to THIS via R
	const CIVec& getRelatedCache ( const TRole* R ) const { return pRelatedMap->getRelated(R); }
		/// set the cache of individuals related to THIS via R
	void setRelatedCache ( const TRole* R, const CIVec& v ) { pRelatedMap->setRelated ( R, v ); }
		/// clear RM
	void clearRelatedMap ( void ) { delete pRelatedMap; pRelatedMap = new TRelatedMap(); }

	// save/load interface; implementation is in SaveLoad.cpp

		/// save entry
	virtual void Save ( SaveLoadManager& m ) const;
		/// load entry
	virtual void Load ( SaveLoadManager& m );
}; // TIndividual

#endif

