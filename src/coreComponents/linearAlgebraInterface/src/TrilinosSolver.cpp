/*
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Copyright (c) 2019, Lawrence Livermore National Security, LLC.
 *
 * Produced at the Lawrence Livermore National Laboratory
 *
 * LLNL-CODE-746361
 *
 * All rights reserved. See COPYRIGHT for details.
 *
 * This file is part of the GEOSX Simulation Framework.
 *
 * GEOSX is a free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License (as published by the
 * Free Software Foundation) version 2.1 dated February 1999.
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */

/*
 * TrilinosSolver.cpp
 *
 *  Created on: Aug 9, 2018
 *  Author: Matthias Cremon
 */

// BEGIN_RST_NARRATIVE TrilinosSolver.rst
// ==============================
// Trilinos Solver
// ==============================
// This class implements solvers from the Trilinos library. Iterative solvers come from
// the AztecOO package, and direct solvers from the Amesos package.

// Include the corresponding header file.
#include "TrilinosSolver.hpp"
#include "LinearSolverParameters.hpp"

// Put everything under the geosx namespace.
namespace geosx
{

// ----------------------------
// Constructors
// ----------------------------

// """""""""""""""""""""""""""""""""""""""""""""""""""""""""
// Constructor
// """""""""""""""""""""""""""""""""""""""""""""""""""""""""

TrilinosSolver::TrilinosSolver( LinearSolverParameters const & parameters )
  :
  m_parameters( parameters )
{}


// ----------------------------
// Top-Level Solver
// ----------------------------
// We switch between different solverTypes here

void TrilinosSolver::solve( EpetraMatrix & mat,
                            EpetraVector & sol,
                            EpetraVector & rhs )
{
  if( m_parameters.scaling.useRowScaling )
  {
    Epetra_FECrsMatrix * mat_ptr = mat.unwrappedPointer();
    Epetra_MultiVector * rhs_ptr = rhs.unwrappedPointer();

    Epetra_Vector scaling( mat_ptr->RowMap() );
    mat_ptr->InvRowSums( scaling );
    mat_ptr->LeftScale( scaling );

    Epetra_MultiVector tmp( *rhs_ptr );
    rhs_ptr->Multiply( 1.0, scaling, tmp, 0.0 );
  }

  if( m_parameters.solverType == "direct" )
  {
    solve_direct( mat, sol, rhs );
  }
  else
  {
    solve_krylov( mat, sol, rhs );
  }
}

// ----------------------------
// Direct solver
// ----------------------------

void TrilinosSolver::solve_direct( EpetraMatrix & mat,
                                   EpetraVector & sol,
                                   EpetraVector & rhs )
{
  // Create Epetra linear problem and instantiate solver.
  Epetra_LinearProblem problem( mat.unwrappedPointer(),
                                sol.unwrappedPointer(),
                                rhs.unwrappedPointer() );

  // Instantiate the Amesos solver.
  Amesos_BaseSolver* solver;
  Amesos Factory;

  // Select KLU solver (only one available as of 9/20/2018)
  solver = Factory.Create( "Klu", problem );

  // Factorize the matrix
  solver->SymbolicFactorization();
  solver->NumericFactorization();

  // Solve the system
  solver->Solve();

  // Basic output
  if( m_parameters.verbosity > 0 )
  {
    solver->PrintStatus();
    solver->PrintTiming();
  }
}


// ----------------------------
// Iterative solver
// ----------------------------

void TrilinosSolver::solve_krylov( EpetraMatrix & mat,
                                   EpetraVector & sol,
                                   EpetraVector & rhs )
{
  // Create Epetra linear problem.
  Epetra_LinearProblem problem( mat.unwrappedPointer(),
                                sol.unwrappedPointer(),
                                rhs.unwrappedPointer() );

  // Instantiate the AztecOO solver.
  AztecOO solver( problem );

  // Choose the solver type
  if( m_parameters.solverType == "gmres" )
  {
    solver.SetAztecOption( AZ_solver, AZ_gmres );
    solver.SetAztecOption( AZ_kspace, m_parameters.krylov.maxRestart );
  }
  else if( m_parameters.solverType == "bicgstab" )
  {
    solver.SetAztecOption( AZ_solver, AZ_bicgstab );
  }
  else if( m_parameters.solverType == "cg" )
  {
    solver.SetAztecOption( AZ_solver, AZ_cg );
  }
  else
    GEOS_ERROR( "The requested linear solverType doesn't seem to exist" );

  // Create a null pointer to an ML amg preconditioner
  std::unique_ptr<ML_Epetra::MultiLevelPreconditioner> ml_preconditioner;

  // Choose the preconditioner type
  if( m_parameters.preconditionerType == "none" )
  {
    solver.SetAztecOption( AZ_precond, AZ_none );
  }
  else if( m_parameters.preconditionerType == "jacobi" )
  {
    solver.SetAztecOption( AZ_precond, AZ_Jacobi );
  }
  else if( m_parameters.preconditionerType == "ilu" )
  {
    solver.SetAztecOption( AZ_precond, AZ_dom_decomp );
    solver.SetAztecOption( AZ_overlap, m_parameters.dd.overlap );
    solver.SetAztecOption( AZ_subdomain_solve, AZ_ilu );
    solver.SetAztecOption( AZ_graph_fill, m_parameters.ilu.fill );
  }
  else if( m_parameters.preconditionerType == "icc" )
  {
    solver.SetAztecOption( AZ_precond, AZ_dom_decomp );
    solver.SetAztecOption( AZ_overlap, m_parameters.dd.overlap );
    solver.SetAztecOption( AZ_subdomain_solve, AZ_icc );
    solver.SetAztecOption( AZ_graph_fill, m_parameters.ilu.fill );
  }
  else if( m_parameters.preconditionerType == "ilut" )
  {
    solver.SetAztecOption( AZ_precond, AZ_dom_decomp );
    solver.SetAztecOption( AZ_overlap, m_parameters.dd.overlap );
    solver.SetAztecOption( AZ_subdomain_solve, AZ_ilut );
    solver.SetAztecParam( AZ_ilut_fill, (m_parameters.ilu.fill>0 ? real64( m_parameters.ilu.fill ) : 1.0));
  }
  else if( m_parameters.preconditionerType == "amg" )
  {
    Teuchos::ParameterList list;

    if( m_parameters.amg.isSymmetric )
      ML_Epetra::SetDefaults( "SA", list );
    else
      ML_Epetra::SetDefaults( "NSSA", list );

    std::map<string, string> translate; // maps GEOSX to ML syntax

    translate.insert( std::make_pair( "V", "MGV" ));
    translate.insert( std::make_pair( "W", "MGW" ));
    translate.insert( std::make_pair( "direct", "Amesos-KLU" ));
    translate.insert( std::make_pair( "jacobi", "Jacobi" ));
    translate.insert( std::make_pair( "blockJacobi", "block Jacobi" ));
    translate.insert( std::make_pair( "gaussSeidel", "Gauss-Seidel" ));
    translate.insert( std::make_pair( "blockGaussSeidel", "block Gauss-Seidel" ));
    translate.insert( std::make_pair( "chebyshev", "Chebyshev" ));
    translate.insert( std::make_pair( "ilu", "ILU" ));
    translate.insert( std::make_pair( "ilut", "ILUT" ));

    list.set( "ML output", m_parameters.verbosity );
    list.set( "max levels", m_parameters.amg.maxLevels );
    list.set( "aggregation: type", "Uncoupled" );
    list.set( "PDE equations", m_parameters.dofsPerNode );
    list.set( "smoother: sweeps", m_parameters.amg.numSweeps );
    list.set( "prec type", translate[m_parameters.amg.cycleType] );
    list.set( "smoother: type", translate[m_parameters.amg.smootherType] );
    list.set( "coarse: type", translate[m_parameters.amg.coarseType] );

    //TODO: add user-defined null space / rigid body mode support
    //list.set("null space: type","pre-computed");
    //list.set("null space: vectors",&rigid_body_modes[0]);
    //list.set("null space: dimension", n_rbm);

    ml_preconditioner.reset( new ML_Epetra::MultiLevelPreconditioner( *mat.unwrappedPointer(), list ));
    solver.SetPrecOperator( ml_preconditioner.get() );
  }
  else
    GEOS_ERROR( "The requested preconditionerType doesn't seem to exist" );

  // Ask for a convergence normalized by the right hand side
  solver.SetAztecOption( AZ_conv, AZ_rhs );

  // Control output
  switch( m_parameters.verbosity )
  {
  case 1:
    solver.SetAztecOption( AZ_output, AZ_summary );
    solver.SetAztecOption( AZ_diagnostics, AZ_all );
    break;
  case 2:
    solver.SetAztecOption( AZ_output, AZ_all );
    solver.SetAztecOption( AZ_diagnostics, AZ_all );
    break;
  default:
    solver.SetAztecOption( AZ_output, AZ_none );
  }

  // Actually solve
  solver.Iterate( m_parameters.krylov.maxIterations,
                  m_parameters.krylov.tolerance );

  //TODO: should we return performance feedback to have GEOSX pretty print details?:
  //      i.e. iterations to convergence, residual reduction, etc.
}

} // end geosx namespace
