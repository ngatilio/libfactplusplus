/* This file is part of the FaCT++ DL reasoner
Copyright (C) 2003-2007 by Dmitry Tsarkov

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*/

/*******************************************************\
|* Implementation of taxonomy building for the FaCT++  *|
\*******************************************************/

#include "Reasoner.h"
#include "DLConceptTaxonomy.h"
#include "procTimer.h"
#include "globaldef.h"
#include "logging.h"
#include "cpm.h"

using namespace std;

/********************************************************\
|* 			Implementation of class Taxonomy			*|
\********************************************************/

bool DLConceptTaxonomy :: testSub ( const TConcept* p, const TConcept* q )
{
	assert ( p != NULL );
	assert ( q != NULL );

	// FIXME!! check this later on when REAL nominals appears
	if ( q->isSingleton() )
		return false;

	if ( LLM.isWritable(llTaxTrying) )
		LL << "\nTAX: trying '" << p->getName() << "' [= '" << q->getName() << "'... ";

	if ( tBox.testSortedNonSubsumption ( p, q ) )
	{
		if ( LLM.isWritable(llTaxTrying) )
			LL << "NOT holds (sorted result)";

		return false;
	}

	switch ( tBox.testCachedNonSubsumption ( p, q ) )
	{
	case csValid:	// cached result: satisfiable => non-subsumption
		if ( LLM.isWritable(llTaxTrying) )
			LL << "NOT holds (cached result)";

		++nCachedNegative;
		return false;

	case csInvalid:	// cached result: unsatisfiable => subsumption holds
		if ( LLM.isWritable(llTaxTrying) )
			LL << "holds (cached result)";

		++nCachedPositive;
		return true;

	default:		// need extra tests
		if ( LLM.isWritable(llTaxTrying) )
			LL << "wasted cache test";

		break;
	}

	// really performed test
	bool res = tBox.isSubHolds ( p, q );

	// update statistic
	++nTries;

	if ( res )
		++nPositives;
	else
		++nNegatives;

	return res;
}

void DLConceptTaxonomy :: print ( std::ostream& o ) const
{
#ifdef __PRINT_FO_WHILE_CLASSIFY
	o << "There were " << nPrintedTasks << " tasks printed with total time "
	  << tOutput << " sec\n";
#endif
	o << "Totally " << nTries << " subsumption tests was made\nAmong them ";

	unsigned int n = ( nTries ? nTries : 1 );

	o << nPositives << " (" << (unsigned long)(nPositives*100/n) << "%) successfull\n";
	o << "Besides that " << nCachedPositive << " successfull and " << nCachedNegative
	  << " unsuccessfull subsumption tests were cached\n";

	o << "Current efficiency (wrt Brute-force) is " << nEntries*(nEntries-1)/n << "\n";

	Taxonomy::print(o);
}

// Baader procedures
void DLConceptTaxonomy :: searchBaader ( bool upDirection, TaxonomyVertex* cur )
{
	// label 'visited'
	cur->setChecked();

	bool noPosSucc = true;

	// check if there are positive successors; use DFS on them.
	for ( TaxonomyVertex::iterator p = cur->begin(upDirection), p_end = cur->end(upDirection); p < p_end; ++p )
		if ( enhancedSubs ( upDirection, *p ) )
		{
			if ( !(*p)->isChecked() )
				searchBaader ( upDirection, *p );

			noPosSucc = false;
		}

	// in case current node is unchecked (no BOTTOM node) -- check it explicitely
	if ( !cur->isValued() )
		testSubsumption ( upDirection, cur );

	// mark labelled leaf node as a parent
	if ( noPosSucc && cur->getValue() )
		Current->addNeighbour ( !upDirection, cur );
}

bool DLConceptTaxonomy :: enhancedSubs ( bool upDirection, TaxonomyVertex* cur )
{
	if ( cur->isValued() )
		return cur->getValue();

	// for going up only
	// if not successor of up -- return false
	if ( upDirection && !cur->isCommon() )
		return cur->setValued(false);

	// need to be valued -- check all parents
	// propagate false
	for ( TaxonomyVertex::iterator p = cur->begin(!upDirection), p_end = cur->end(!upDirection); p < p_end; ++p )
		if ( !enhancedSubs ( upDirection, *p ) )
			return cur->setValued(false);

	// all subsumptions holds -- test current for subsumption
	return testSubsumption ( upDirection, cur );
}

bool DLConceptTaxonomy :: testSubsumption ( bool upDirection, TaxonomyVertex* cur )
{
	const TConcept* testC = static_cast<const TConcept*>(cur->getPrimer());
	if ( upDirection )
		return cur->setValued ( testSub ( testC, curConcept() ) );
	else	// top-down phase: if C is NATS and neither C no CUR are non-prim -- return false
			//FIXME!! need more thinking (see bio1/rtms1 test cases in _BUGS_)
		if ( 0 && useCompletelyDefined && testC->isNaTS()
			&& !testC->isNonPrimitive() && !curConcept()->isNonPrimitive() )
			return cur->setValued(false);
		else
			return cur->setValued ( testSub ( curConcept(), testC ) );
}

bool DLConceptTaxonomy :: propagateUp ( void )
{
	const bool upDirection = true;

	// including node always have some parents (TOP at least)
	TaxonomyVertex::iterator p = Current->begin(upDirection), p_end = Current->end(upDirection);

	TaxonomyLink aux;	// aux set for the verteces in ...
	unsigned int nCommon = 1;	// number of common parents

	// define possible successors of the node
	(*p)->propagateOne(Common);
	TaxonomyVertex::clearChecked();

	for ( ++p; p < p_end; ++p )
	{
		if ( (*p)->noNeighbours(!upDirection) )
			return true;
		if ( Common.empty() )
			return true;

		++nCommon;
		(*p)->propagateOne(aux);
		TaxonomyVertex::clearChecked();

		// clear all non-common nodes that are in Common (visited on a previous run)
		TaxonomyLink::iterator q, q_end;
		for ( q = Common.begin(), q_end = Common.end(); q < q_end; ++q )
			(*q)->correctCommon(nCommon);

		// put all common elements to Common here
		Common.clear();
		for ( q = aux.begin(), q_end = aux.end(); q < q_end; ++q )
			if ( (*q)->correctCommon(nCommon) )
				Common.push_back(*q);
	}

	return false;
}

void
DLConceptTaxonomy :: clearCommon ( void )
{
	for ( TaxonomyLink::iterator p = Common.begin(), p_end = Common.end(); p < p_end; ++p )
		(*p)->clearCommon();
	Common.clear();
}

/********************************************************\
|* 			Implementation of class Reasoner			*|
\********************************************************/

// vectors for Completely defined, Non-CD and Non-primitive concepts
TBox::ConceptVector arrayCD, arrayNoCD, arrayNP;

template<class Iterator>
unsigned int fillArrays ( Iterator begin, Iterator end )
{
	for ( Iterator p = begin; p < end; ++p )
		switch ( (*p)->getClassTag() )
		{
		case cttTrueCompletelyDefined:
			arrayCD.push_back(*p);
			break;
		default:
			arrayNoCD.push_back(*p);
			break;
		case cttNonPrimitive:
		case cttHasNonPrimitiveTS:
			arrayNP.push_back(*p);
			break;
		}

	return end - begin;
}

void TBox :: createTaxonomy ( bool needIndividual )
{
	bool needConcept = !needIndividual;

	// here we sure that ontology is consistent

	if ( pTax == NULL )	// first run
	{
		prepareSubReasoning();
		pTax = new DLConceptTaxonomy ( pTop, pBottom, *this, GCIs );
		needConcept |= needIndividual;	// together with concepts
	}
	else
	{
		assert ( needIndividual );
		pTax->deFinalise();
	}

	if ( verboseOutput )
		std::cerr << "Processing query...";

	TsProcTimer locTimer;
	locTimer.Start();

	// calculate number of items to be classified
	unsigned int nItems = 0;

	// fills collections
	arrayCD.clear();
	arrayNoCD.clear();
	arrayNP.clear();

	if ( needConcept )
		nItems += fillArrays ( c_begin(), c_end() );
	if ( needIndividual )
		nItems += fillArrays ( i_begin(), i_end() );

	// taxonomy progress
	if ( !pMonitor && verboseOutput )
		setProgressMonitor(new ConsoleProgressMonitor);

	if ( pMonitor )
	{
		pMonitor->setClassificationStarted(nItems);
		pTax->setProgressIndicator(pMonitor);
	}

//	sort ( arrayCD.begin(), arrayCD.end(), TSDepthCompare() );
	classifyConcepts ( arrayCD, true, "completely defined" );
//	sort ( arrayNoCD.begin(), arrayNoCD.end(), TSDepthCompare() );
	classifyConcepts ( arrayNoCD, true, "regular" );
//	sort ( arrayNP.begin(), arrayNP.end(), TSDepthCompare() );
	classifyConcepts ( arrayNP, false, "non-primitive" );

	if ( pMonitor )
		pMonitor->setFinished();
	pTax->finalise();

	locTimer.Stop();
	if ( verboseOutput )
		std::cerr << " done in " << locTimer << " seconds\n";

	if ( verboseOutput && needIndividual )
	{
		std::ofstream of ( "Taxonomy.log" );
		pTax->print (of);
	}
}

void TBox :: classifyConcepts ( const ConceptVector& collection, bool curCompletelyDefined,
								const char* type ATTR_UNUSED )
{
	// set CD for taxonomy
	pTax->setCompletelyDefined (curCompletelyDefined);

	if ( LLM.isWritable(llStartCfyConcepts) )
		LL << "\n\n---Start classifying " << type << " concepts";

	unsigned int n = 0;

	for ( ConceptVector::const_iterator q = collection.begin(), q_end = collection.end(); q < q_end; ++q )
		// check if concept is already classified
		if ( !isCancelled() && !(*q)->isClassified () /*&& (*q)->isClassifiable(curCompletelyDefined)*/ )
		{
			pTax->classifyEntry (*q);	// need to classify concept
			if ( (*q)->isClassified() )
				++n;
		}

	if ( LLM.isWritable(llStartCfyConcepts) )
		LL << "\n---Done: " << n << " " << type << " concepts classified";
}
