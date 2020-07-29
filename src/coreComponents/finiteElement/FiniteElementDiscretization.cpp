/*
 * ------------------------------------------------------------------------------------------------------------
 * SPDX-License-Identifier: LGPL-2.1-only
 *
 * Copyright (c) 2018-2019 Lawrence Livermore National Security LLC
 * Copyright (c) 2018-2019 The Board of Trustees of the Leland Stanford Junior University
 * Copyright (c) 2018-2019 Total, S.A
 * Copyright (c) 2019-     GEOSX Contributors
 * All right reserved
 *
 * See top level LICENSE, COPYRIGHT, CONTRIBUTORS, NOTICE, and ACKNOWLEDGEMENTS files for details.
 * ------------------------------------------------------------------------------------------------------------
 */

/**
 * @file FiniteElementSpace.cpp
 */

#include "FiniteElementDiscretization.hpp"

#include "mesh/CellElementSubRegion.hpp"
#include "managers/DomainPartition.hpp"
#include "managers/ObjectManagerBase.hpp"
#include "mesh/NodeManager.hpp"
#include "managers/NumericalMethodsManager.hpp"
#include "codingUtilities/Utilities.hpp"
#include "common/TimingMacros.hpp"

// TODO make this not dependent on this header...need better key implementation

namespace geosx
{
using namespace dataRepository;
using namespace finiteElement;


FiniteElementDiscretization::FiniteElementDiscretization( std::string const & name, Group * const parent ):
  Group( name, parent )
{
  setInputFlags( InputFlags::OPTIONAL_NONUNIQUE );

  registerWrapper( viewKeyStruct::orderString, &m_order )->
    setInputFlag( InputFlags::REQUIRED );

  registerWrapper( viewKeyStruct::formulationString, &m_formulation )->
    setInputFlag( InputFlags::OPTIONAL )->
    setApplyDefaultValue( "default" );
}

FiniteElementDiscretization::~FiniteElementDiscretization()
{}


void FiniteElementDiscretization::PostProcessInput()
{
  GEOSX_ERROR_IF( m_order!=1, "Higher order finite element spaces are currently not supported.");
  GEOSX_ERROR_IF( m_formulation!="default", "Only standard element formulations are currently supported.");
}

std::unique_ptr<FiniteElementBase>
FiniteElementDiscretization::factory( string const & parentElementShape ) const
{
  std::unique_ptr<FiniteElementBase> rval;
  if( m_order==1 )
  {
    if( parentElementShape ==  finiteElement::ParentElementTypeStrings::Hexahedron )
    {
      rval = std::make_unique<Hexahedron_Lagrange1_GaussLegendre2>();
    }
    else if( parentElementShape == finiteElement::ParentElementTypeStrings::Tetrahedon )
    {
      rval = std::make_unique<LinearTetrahedronShapeFunctionKernel>();
    }
    else if( parentElementShape == finiteElement::ParentElementTypeStrings::Prism )
    {
      rval = std::make_unique<BiLinearWedgeShapeFunctionKernel>();
    }
    else if( parentElementShape == finiteElement::ParentElementTypeStrings::Pyramid )
    {
      rval = std::make_unique<PyramidShapeFunctionKernel>();
    }
    else if( parentElementShape == finiteElement::ParentElementTypeStrings::Quadralateral )
    {
      rval = std::make_unique<BiLinearQuadrilateralFaceShapeFunctionKernel>();
    }
    else if( parentElementShape == finiteElement::ParentElementTypeStrings::Triangle )
    {
      rval = std::make_unique<LinearTriangleFaceShapeFunctionKernel>();
    }
    else
    {
      GEOSX_ERROR( "" );
    }
  }
  return rval;
}

REGISTER_CATALOG_ENTRY( Group, FiniteElementDiscretization, std::string const &, Group * const )

} /* namespace geosx */
