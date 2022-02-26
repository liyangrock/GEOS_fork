/*
 * ------------------------------------------------------------------------------------------------------------
 * SPDX-License-Identifier: LGPL-2.1-only
 *
 * Copyright (c) 2018-2020 Lawrence Livermore National Security LLC
 * Copyright (c) 2018-2020 The Board of Trustees of the Leland Stanford Junior University
 * Copyright (c) 2018-2020 TotalEnergies
 * Copyright (c) 2019-     GEOSX Contributors
 * All rights reserved
 *
 * See top level LICENSE, COPYRIGHT, CONTRIBUTORS, NOTICE, and ACKNOWLEDGEMENTS files for details.
 * ------------------------------------------------------------------------------------------------------------
 */

/**
 * @file SymbolicFunction.cpp
 */

#include "SymbolicFunction.hpp"
#include "common/DataTypes.hpp"

namespace geosx
{

namespace dataRepository
{
namespace keys
{
string const variableNames = "variableNames";
string const expression = "expression";
}
}

using namespace dataRepository;

SymbolicFunction::SymbolicFunction( const string & name,
                                    Group * const parent ):
  FunctionBase( name, parent ),
  parserContext(),
  parserExpression()
{
  registerWrapper( keys::variableNames, &m_variableNames ).
    setInputFlag( InputFlags::REQUIRED ).
    setSizedFromParent( 0 ).
    setDescription( "List of variables in expression.  The order must match the evaluate argument" );

  registerWrapper( keys::expression, &m_expression ).
    setInputFlag( InputFlags::REQUIRED ).
    setSizedFromParent( 0 ).
    setDescription( "Symbolic math expression" );
}


SymbolicFunction::~SymbolicFunction()
{}

class GeosxMathpressoLogger : public mathpresso::OutputLog
{
public:

  explicit GeosxMathpressoLogger( string name )
    : m_name( std::move( name ) )
  {}

  virtual ~GeosxMathpressoLogger() override
  {
    string const s = m_stream.str();
    if( !s.empty() )
    {
      GEOSX_LOG_RANK_0( GEOSX_FMT( "{} '{}': JIT compiler produced the following output:\n{}",
                                   SymbolicFunction::catalogName(), m_name, s ) );
    }
  }

  virtual void log( unsigned int type,
                    unsigned int line,
                    unsigned int column,
                    const char * const message,
                    size_t const GEOSX_UNUSED_PARAM( size ) ) override
  {
    switch( type )
    {
      case kMessageError:
      {
        m_stream << GEOSX_FMT( "[ERROR]: {} (line {}, column {})\n", message, line, column );
        break;
      }
      case kMessageWarning:
      {
        m_stream << GEOSX_FMT( "[WARNING]: {} (line {}, column {})\n", message, line, column );
        break;
      }
      default:
      {
        m_stream << GEOSX_FMT( "[OTHER]\n{}", message );
        break;
      }
    }
  }

private:

  string m_name;
  std::ostringstream m_stream;

};

void SymbolicFunction::initializeFunction()
{
  // Register variables
  for( localIndex ii=0; ii<m_variableNames.size(); ++ii )
  {
    parserContext.addVariable( m_variableNames[ii].c_str(), static_cast< int >(ii * sizeof(double)));
  }

  // Add built in constants/functions (PI, E, sin, cos, ceil, exp, etc.),
  // compile
  parserContext.addBuiltIns();
  mathpresso::Error const err = [&]
  {
    GeosxMathpressoLogger outputLog( getName() );
    return parserExpression.compile( parserContext, m_expression.c_str(), mathpresso::kNoOptions, &outputLog );
  }();
  GEOSX_ERROR_IF( err != mathpresso::kErrorOk, "MathPresso JIT Compiler Error" );
}

REGISTER_CATALOG_ENTRY( FunctionBase, SymbolicFunction, string const &, Group * const )

} // namespace geosx
