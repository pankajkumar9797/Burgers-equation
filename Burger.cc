/* ---------------------------------------------------------------------
 *
 * Copyright (C) 1999 - 2013 by the deal.II authors
 *
 * This file is part of the deal.II library.
 *
 * The deal.II library is free software; you can use it, redistribute
 * it, and/or modify it under the terms of the GNU Lesser General
 * Public License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * The full text of the license can be found in the file LICENSE at
 * the top level of the deal.II distribution.
 *
 * ---------------------------------------------------------------------

 *
 * Author: Pankaj Kumar, MSc 2017.
 */

#include <deal.II/base/utilities.h>
#include <deal.II/base/quadrature_lib.h>
#include <deal.II/base/function.h>
#include <deal.II/base/tensor_function.h>
#include <deal.II/base/logstream.h>
#include <deal.II/lac/vector.h>
#include <deal.II/lac/full_matrix.h>
#include <deal.II/lac/compressed_sparsity_pattern.h>
#include <deal.II/lac/sparse_matrix.h>
#include <deal.II/lac/solver_gmres.h>
#include <deal.II/lac/solver_cg.h>
#include <deal.II/lac/precondition.h>
#include <deal.II/lac/constraint_matrix.h>
#include <deal.II/grid/tria.h>
#include <deal.II/grid/grid_generator.h>
#include <deal.II/grid/grid_refinement.h>
#include <deal.II/grid/grid_out.h>
#include <deal.II/grid/tria_accessor.h>
#include <deal.II/grid/tria_iterator.h>
#include <deal.II/dofs/dof_handler.h>
#include <deal.II/dofs/dof_accessor.h>
#include <deal.II/dofs/dof_tools.h>
#include <deal.II/fe/fe_system.h>
#include <deal.II/fe/fe_q.h>
#include <deal.II/fe/fe_values.h>
#include <deal.II/numerics/data_out.h>
#include <deal.II/numerics/vector_tools.h>
#include <deal.II/numerics/error_estimator.h>
#include <deal.II/numerics/solution_transfer.h>
#include <deal.II/numerics/matrix_tools.h>

#include <deal.II/lac/trilinos_precondition.h>

#include <deal.II/numerics/data_out.h>
#include <fstream>
#include <iostream>
#include <cmath>
#include <sstream>
#include <deal.II/base/logstream.h>



using namespace dealii;

template <int dim>
class Burger
{
public:
  Burger ();
  ~Burger();
  void run ();

private:
  void make_grid ();
  void setup_system();
  void assemble_system_2 ();
  void solve ();
  void refine_grid (const unsigned int min_grid_level, const unsigned int max_grid_level);
  void output_results () const;

  double solution_bdf(
    const double& sol_val,
    const double& rhs_val,
    const double& v_val,
    const Tensor<1, dim>& sol_grad,
    const Tensor<1, dim>& v_grad,
    const Tensor<1, dim>& beta
  ) const;

  double solution_bdf1(
    const double& sol_old,
    const double& sol_old_old
  ) const;

  double advection_cell_operator(
    const double& u_val,
    const double& v_val,
    const Tensor<1, dim>& u_grad,
    const Tensor<1, dim>& v_grad,
    const Tensor<1, dim>& beta
  ) const;

  double advection_face_operator(
    const double& u_val,
    const double& v_val,
    const Tensor<1, dim>& beta,
    const Tensor<1, dim>& normal
  ) const;

  double lhs_operator(
    const double& u_val,
    const double& v_val,
    const Tensor<1, dim>& u_grad,
    const Tensor<1, dim>& v_grad,
    const double& alpha,
    const Tensor<1, dim>& beta
  ) const;

  double streamline_diffusion(
    const Tensor<1, dim>& u_grad,
    const Tensor<1, dim>& v_grad,
    const Tensor<1, dim>& beta
  ) const;

  Triangulation<dim>   triangulation;

  FESystem<dim>        fe;

  DoFHandler<dim>      dof_handler;

  ConstraintMatrix     constraints;

  SparsityPattern      sparsity_pattern;
  SparseMatrix<double> system_matrix;

  Vector<double>       old_solution;
  Vector<double>       old_old_solution;
  Vector<double>       solution;
  Vector<double>       system_rhs;

  unsigned int         timestep_number;
  double               time_step;
  double               time;

  double               theta_imex;
  double               theta_skew;

  const double         nu = 1.0;
};


template<int dim>
class RightHandSide : public Function<dim>
{
public:
  RightHandSide (const double& time)
    :
    Function<dim>(dim),
    period (0.2),
    time(time)
  {}
  virtual double value (const Point<dim> &p,
                        const unsigned int component = 0) const;
  virtual void vector_value (const Point<dim>  &points,
		  Vector<double> &value) const;
private:
  const double period;
  const double time ;
};

template<int dim>
double RightHandSide<dim>::value (const Point<dim> &p,
                                  const unsigned int component) const
{

  Assert (dim == 2, ExcNotImplemented());

//  const double time = this->get_time();
  const double point_within_period = (time/period - std::floor(time/period));


  switch(component){
  	  case 0:
		  if ((point_within_period >= 0.0) && (point_within_period <= 0.2))
			{
			  if ((p[0] > 0.5) && (p[1] > -0.5))
				return 1;
			  else
				return 0;
			}
		  else if ((point_within_period >= 0.5) && (point_within_period <= 0.7))
			{
			  if ((p[0] > -0.5) && (p[1] > 0.5))
				return 1;
			  else
				return 0;
			}
		  else
			return 0;

  	  case 1:
		  if ((point_within_period >= 0.2) && (point_within_period <= 0.4))
			{
			  if ((p[0] > 0.5) && (p[1] > -0.5))
				return 1;
			  else
				return 0;
			}
		  else if ((point_within_period >= 0.7) && (point_within_period <= 0.9))
			{
			  if ((p[0] > -0.5) && (p[1] > 0.5))
				return 1;
			  else
				return 0;
			}
		  else
			return 0;

      default: return 0;
  }

}

template <int dim>
void
RightHandSide<dim>::vector_value (const Point<dim>  &points,
		 	 	 	 	 	 	 Vector<double> &values) const
{

	for (unsigned int c=0; c<this->n_components; ++c)
	  values(c) = RightHandSide<dim>::value (points, c);

}

template <int dim>
void right_hand_side (const std::vector<Point<dim> > &points,
                      std::vector<Tensor<1, dim> >   &values)
{
  Assert (values.size() == points.size(),
          ExcDimensionMismatch (values.size(), points.size()));
  Assert (dim >= 2, ExcNotImplemented());
  Point<dim> point_1, point_2;
  point_1(0) = 0.5;
  point_2(0) = -0.5;
  for (unsigned int point_n = 0; point_n < points.size(); ++point_n)
    {
      if (((points[point_n]-point_1).norm_square() < 0.2*0.2) ||
          ((points[point_n]-point_2).norm_square() < 0.2*0.2))
        values[point_n][0] = 1.0;
      else
        values[point_n][0] = 0.0;
      if (points[point_n].norm_square() < 0.2*0.2)
        values[point_n][1] = 1.0;
      else
        values[point_n][1] = 0.0;
    }
}

/*

template<int dim>
class RightHandSide1 : public Function<dim>
{
public:
  RightHandSide1 (const double& time) : Function<dim>(), time(time) {}
  virtual double value (const Point<dim> &p,
                        const unsigned int component = 0) const;
  const double time;

};

template<int dim>
double RightHandSide1<dim> ::value(const Point<dim> &p,
		                    const unsigned int component) const{

	return std::exp(-time)*(5 - 3*p[0]*p[0] -3*p[1]*p[1] + p[0]*p[0]*p[1]*p[1]);
}
*/
//////////////////////////

template <int dim>
class RightHandSide1 : public Function<dim>
{
public:
  RightHandSide1 () : Function<dim>(dim) {}
  virtual double value (const Point<dim>   &p,
						const unsigned int  component = 0) const;
  virtual void vector_value (const Point<dim> &p,
							 Vector<double>   &value) const;
};
template <int dim>
double
RightHandSide1<dim>::value (const Point<dim>  &p  ,
						   const unsigned int /*component */) const
{
  return 0;
}
template <int dim>
void
RightHandSide1<dim>::vector_value (const Point<dim> &p,
								  Vector<double>   &values) const
{
  for (unsigned int c=0; c<this->n_components; ++c)
	values(c) = RightHandSide1<dim>::value (p, c);
}


template <int dim>
class BubbleGauss : public dealii::Function<dim> {
public:
  BubbleGauss(const double& amplitude = 1, const double& sigma = 5, const Point<dim>& center = Point<dim>());
  double amplitude;
  double sigma;
  Point<dim> center;
  virtual double value(const Point<dim>& p, const unsigned component = 0) const;
  virtual void vector_value (const Point<dim>  &points,
		  Vector<double> &value) const;


};

template<int dim>
BubbleGauss<dim>::BubbleGauss(const double& amplitude, const double& sigma, const Point<dim>& center)  :  Function<dim>(dim), amplitude(amplitude), sigma(sigma), center(center) {

}

template<int dim>
double BubbleGauss<dim>::value(const Point<dim>& p, const unsigned component ) const {

	const double& r2 = (p - center).norm_square();

    switch (component) {
      case 0:  return  2*(p[0]*p[0] - 1)*(p[1]*p[1] - 1)*(p[0]*(p[1]*p[1] - 1) + p[1]*(p[0]*p[0] - 1))
    		            -1.0*(2.0*(p[1]*p[1] - 1) + 2*(p[0]*p[0] - 1)) ;
                                   break;//amplitude * std::exp(-sigma * r2); break;

      case 1:  return  2*(p[0]*p[0] - 1)*(p[1]*p[1] - 1)*(p[0]*(p[1]*p[1] - 1) + p[1]*(p[0]*p[0] - 1))
	                    -1.0*(2.0*(p[1]*p[1] - 1) + 2*(p[0]*p[0] - 1)) ;
                                   break;//0.1; break;
      default: return 0;
    }

}

template <int dim>
void
BubbleGauss<dim>::vector_value (const Point<dim>  &points,
		 	 	 	 	 	 	 Vector<double> &values) const
{

	for (unsigned int c=0; c<this->n_components; ++c)
	  values(c) = BubbleGauss<dim>::value (points, c);

}

template <int dim>
class ExactSolution : public dealii::Function<dim> {
public:
  ExactSolution(): Function<dim>(dim){} ;
  virtual void vector_value (const Point<dim>  &points,
		  Vector<double> &value) const;

};


template <int dim>
void
ExactSolution<dim>::vector_value (const Point<dim>  &p,
		 	 	 	 	 	 	 Vector<double> &values) const
{
    Assert (values.size() == dim,
            ExcDimensionMismatch (values.size(), dim));

    values(0) = (p[0]*p[0] - 1)*(p[1]*p[1] - 1) ;
    values(1) = (p[0]*p[0] - 1)*(p[1]*p[1] - 1) ;

}
/////////////////////////////////
template <int dim>
class BoundaryValues : public Function<dim>
{
public:
  virtual double value (const Point<dim>   &p,
                        const unsigned int  component = 0) const;
};







template<int dim>
double BoundaryValues<dim>::value (const Point<dim> &/*p*/,
                                   const unsigned int component) const
{
    Assert(component == 0, ExcInternalError());
    return 0;
}

template<int dim>
double Burger<dim>::solution_bdf1(
  const double& sol_old,
  const double& sol_old_old
) const{
  return(
	+ (2.0)*sol_old - (0.5)*sol_old_old
	);
}

template<int dim>
double Burger<dim>::solution_bdf(
  const double& sol_val,
  const double& rhs_val,
  const double& v_val,
  const Tensor<1, dim>& sol_grad,
  const Tensor<1, dim>& v_grad,
  const Tensor<1, dim>& beta
) const{
  return(
	+ sol_val*v_val
	- time_step*contract(beta , sol_grad )* v_val
//    + time_step * nu * sol_grad * v_grad
    + time_step * rhs_val*v_val
  );
}

template<int dim>
double Burger<dim>::advection_cell_operator(
  const double& u_val,
  const double& v_val,
  const Tensor<1, dim>& u_grad,
  const Tensor<1, dim>& v_grad,
  const Tensor<1, dim>& beta
) const {
  return (
    + (1 - theta_skew) * contract(beta, u_grad) * v_val
    + (0 - theta_skew) * contract(beta, v_grad) * u_val
    );
}


template<int dim>
double Burger<dim>::advection_face_operator(
  const double& u_val,
  const double& v_val,
  const Tensor<1, dim>& beta,
  const Tensor<1, dim>& normal
) const {
  return (
    + theta_skew * contract(beta, normal) * v_val * u_val
    );
}

template<int dim>
double Burger<dim>::lhs_operator(
  const double& u_val,
  const double& v_val,
  const Tensor<1, dim>& u_grad,
  const Tensor<1, dim>& v_grad,
  const double& alpha,
  const Tensor<1, dim>& beta
) const {
  return (
    + theta_imex * advection_cell_operator(u_val, v_val, u_grad, v_grad, beta)
    + theta_skew * alpha * (u_grad * v_grad)
    + alpha * (u_grad * v_grad)
    );
}

template<int dim>
double Burger<dim>::streamline_diffusion(
  const Tensor<1, dim>& u_grad,
  const Tensor<1, dim>& v_grad,
  const Tensor<1, dim>& beta
) const {
  const double dt = time_step;
  return dt*dt/6*contract(beta, u_grad) * contract(beta, v_grad);
}

template <int dim>
Burger<dim>::Burger ()
  :
  fe (FE_Q<dim>(1), dim),
  dof_handler (triangulation),
  timestep_number(0),
  time_step(1. / 500),
  time(0),
  theta_imex(0.5),
  theta_skew(0.5)
{}

template <int dim>
Burger<dim>::~Burger (){
  dof_handler.clear ();
}

template <int dim>
void Burger<dim>::make_grid ()
{
//  GridGenerator::hyper_L(triangulation);
  GridGenerator::hyper_cube (triangulation, -1, 1);
  triangulation.refine_global (3);

  std::cout << "   Number of active cells: "
            << triangulation.n_active_cells()
            << std::endl
            << "   Total number of cells: "
            << triangulation.n_cells()
            << std::endl;
}


template <int dim>
void Burger<dim>::setup_system ()
{
  dof_handler.distribute_dofs (fe);

  std::cout << "   Number of degrees of freedom: "
            << dof_handler.n_dofs()
            << std::endl;

  constraints.clear ();
  DoFTools::make_hanging_node_constraints (dof_handler,
                                           constraints);

  VectorTools::interpolate_boundary_values (dof_handler,
                                            0,
                                            ZeroFunction<dim>(dim),
                                            constraints);


  constraints.close();

  DynamicSparsityPattern  c_sparsity(dof_handler.n_dofs());
  DoFTools::make_sparsity_pattern(dof_handler, c_sparsity, constraints, /*keep_constrained_dofs = */ true);

  sparsity_pattern.copy_from(c_sparsity);

  system_matrix.reinit (sparsity_pattern);

  old_solution.reinit(dof_handler.n_dofs());
  old_old_solution.reinit(dof_handler.n_dofs());
  solution.reinit (dof_handler.n_dofs());
  system_rhs.reinit (dof_handler.n_dofs());
}


template <int dim>
void Burger<dim>::assemble_system_2 ()
{
  QGauss<dim>  quadrature_formula(2);

//  const BubbleGauss<dim>  right_hand_side;
  const RightHandSide<dim> right_hand_side(time);
//    const ZeroFunction<dim>   right_hand_side(dim);

  system_matrix = 0;
  system_rhs    = 0;

  FEValues<dim> fe_values (fe, quadrature_formula,
                           update_values   | update_gradients |
                           update_quadrature_points | update_JxW_values);


  const unsigned int   dofs_per_cell = fe.dofs_per_cell;
  const unsigned int   n_q_points    = quadrature_formula.size();

  FullMatrix<double>   cell_matrix (dofs_per_cell, dofs_per_cell);
  Vector<double>       cell_rhs (dofs_per_cell);

  std::vector<types::global_dof_index> local_dof_indices (dofs_per_cell);
  std::vector<Tensor<1, dim> >                 old_values (n_q_points);
  std::vector<Tensor<1, dim> >                 old_old_values (n_q_points);
  std::vector<Tensor<2, dim> >                 old_grad (n_q_points);
  std::vector<double>                          old_div(n_q_points);
  std::vector<double>                          old_old_div(n_q_points);

  std::vector<Vector<double> >      rhs_values (n_q_points, Vector<double>(dim));

  typename DoFHandler<dim>::active_cell_iterator
  cell = dof_handler.begin_active(),
  endc = dof_handler.end();

  for (; cell!=endc; ++cell)
    {
      fe_values.reinit (cell);

      const FEValuesViews::Vector<dim>& fe_vector_values = fe_values[FEValuesExtractors::Vector(0)];

      cell_matrix = 0;
      cell_rhs = 0;
      fe_vector_values.get_function_values (old_solution, old_values);
      fe_vector_values.get_function_gradients(old_solution, old_grad);
      fe_vector_values.get_function_divergences(old_solution, old_div);

      fe_vector_values.get_function_values (old_old_solution, old_old_values);
      fe_vector_values.get_function_divergences(old_old_solution, old_old_div);


      right_hand_side.vector_value_list(fe_values.get_quadrature_points(),
                                  rhs_values);


      for (unsigned int q_index=0; q_index<n_q_points; ++q_index){


          Tensor<1, dim> rhs_val;

          for (int d = 0; d < dim; ++d) {
            rhs_val[d] += right_hand_side.value(fe_values.quadrature_point(q_index), d);
          }

    	  const double& u_star_div = old_div[q_index]; //2*old_div[q_index]    - old_old_div[q_index] ;
    	  const Tensor<1, dim>& u_star     = old_values[q_index]; //2*old_values[q_index] - old_old_values[q_index];

        for (unsigned int i=0; i<dofs_per_cell; ++i)
          {
            const Tensor<1, dim>& u_val   = fe_vector_values.value(i, q_index);
            const Tensor<2, dim>& u_grad  = fe_vector_values.gradient(i, q_index);

            for (unsigned int j=0; j<dofs_per_cell; ++j) {

                const Tensor<1, dim>& v_val   = fe_vector_values.value(j, q_index);
                const Tensor<2, dim>& v_grad  = fe_vector_values.gradient(j, q_index);

              cell_matrix(i,j) += ( u_val * v_val //0.5*(3*u_val * v_val)
            		               +
            		               time_step*contract3(u_star, u_grad, v_val)//u_star*u_grad*v_val
            		               +
            		               0.5*time_step*u_star_div*contract(u_val, v_val)
            		               +
            		               nu*time_step*double_contract(u_grad, v_grad)
                                       )*fe_values.JxW (q_index);
            }
            				//2.0*old_values[q_index]* u_val - 0.5*old_old_values[q_index]* u_val
            cell_rhs(i) += (old_values[q_index]* u_val  + time_step * (rhs_val * u_val)
                               )* fe_values.JxW (q_index);
          }
      }
      cell->get_dof_indices (local_dof_indices);
     constraints.distribute_local_to_global(cell_matrix,
                                          cell_rhs,
                                          local_dof_indices,
                                          system_matrix,
                                          system_rhs);

    }


//  BoundaryValues<dim> boundary_values_function;
//  boundary_values_function.set_time(time);

  std::map<types::global_dof_index,double> boundary_values;
  VectorTools::interpolate_boundary_values (dof_handler,
                                            0,
                                            ZeroFunction<dim>(dim),
                                            boundary_values);
  MatrixTools::apply_boundary_values (boundary_values,
                                      system_matrix,
                                      solution,
                                      system_rhs);

}


template <int dim>
void Burger<dim>::solve ()
{
/*  SolverControl           solver_control (1000, 1e-8 * system_rhs.l2_norm());
  SolverCG<>              solver (solver_control);

  PreconditionSSOR<> preconditioner;//PreconditionIdentity()
  preconditioner.initialize(system_matrix, 1.0);

  solver.solve (system_matrix, solution, system_rhs,
                preconditioner);

*/
	int    vel_max_its     = 5000;
	double vel_eps         = 1e-9;
	int    vel_Krylov_size = 30;

  PreconditionSSOR<> preconditioner;
  preconditioner.initialize(system_matrix, 1.0);

	SolverControl solver_control (vel_max_its, vel_eps*system_rhs.l2_norm());
	{
		SolverGMRES<Vector<double>> gmres1 (solver_control,
						   SolverGMRES<>::AdditionalData (vel_Krylov_size));
		gmres1.solve (system_matrix, solution, system_rhs, preconditioner);
	}



  std::cout << "   " << solver_control.last_step()
            << " GMRES iterations needed to obtain convergence."
            << std::endl;

  constraints.distribute (solution);
}

template <int dim>
void Burger<dim>::refine_grid(const unsigned int min_grid_level,
		                     const unsigned int max_grid_level){

	Vector<float> estimated_error_per_cell(triangulation.n_active_cells());

	KellyErrorEstimator<dim>::estimate(dof_handler,
			                            QGauss<dim-1>(fe.degree+2),
			                            typename FunctionMap<dim>::type(),
			                            solution,
			                            estimated_error_per_cell);
/*
	GridRefinement::refine_and_coarsen_fixed_number(triangulation,
			                                         estimated_error_per_cell,
			                                         0.6, 0.4);
	*/

    GridRefinement::refine_and_coarsen_fixed_number (triangulation,
                                                       estimated_error_per_cell,
                                                       0.5, 0.2);

	if(triangulation.n_levels() > max_grid_level){
		for(typename Triangulation<dim>::active_cell_iterator
				cell = triangulation.begin_active(max_grid_level);
				cell != triangulation.end(); ++cell){
			cell->clear_refine_flag();
		}
	}
	for(typename Triangulation<dim>::active_cell_iterator
			cell = triangulation.begin_active(min_grid_level);
			cell != triangulation.end_active(min_grid_level); ++cell)
		cell->clear_coarsen_flag();

	SolutionTransfer<dim> solution_transfer(dof_handler);

	Vector<double> previous_solution;
	previous_solution = solution;


	triangulation.prepare_coarsening_and_refinement();
	solution_transfer.prepare_for_coarsening_and_refinement(previous_solution);


	triangulation.execute_coarsening_and_refinement();
	setup_system();

	solution_transfer.interpolate(previous_solution, solution);

	constraints.distribute(solution);

}



template <int dim>
void Burger<dim>::output_results () const
{    /*
    DataOut<dim> data_out;
    data_out.attach_dof_handler(dof_handler);
    data_out.add_data_vector(solution, "velocity");
    data_out.build_patches();
    const std::string filename = "solution-"
                                 + Utilities::int_to_string(timestep_number, 3) +
                                 ".vtk";
    std::ofstream output(filename.c_str());
    data_out.write_vtk(output);

 */

    std::vector<std::string> solution_names (dim, "velocity");

    std::vector<DataComponentInterpretation::DataComponentInterpretation>
    data_component_interpretation
    (dim, DataComponentInterpretation::component_is_part_of_vector);

    DataOut<dim> data_out;
    data_out.attach_dof_handler (dof_handler);
    data_out.add_data_vector (solution, solution_names,
                              DataOut<dim>::type_dof_data,
                              data_component_interpretation);
    data_out.build_patches ();
    std::ostringstream filename;
    filename << "solution-"
             << Utilities::int_to_string (timestep_number, 3)
             << ".vtk";
    std::ofstream output (filename.str().c_str());
    data_out.write_vtk (output);
}




template <int dim>
void Burger<dim>::run ()
{
  std::cout << "Solving problem in " << dim << " space dimensions." << std::endl;

  std::ofstream error_out;
  error_out.open("l2_error.dat");


  make_grid();
  setup_system ();
  const BubbleGauss<dim> bubble_gum;
  unsigned int pre_refinement_step = 0;
  const unsigned int n_adaptive_pre_refinement_steps = 4;
  const unsigned int initial_global_refinement = 2;

  Vector<float> difference_per_cell (triangulation.n_active_cells());
  double L2_error ;
  ExactSolution<dim> exact_sol;
  const ComponentSelectFunction<dim> velocity_mask(std::make_pair(0, dim), dim);


start_time_iteration:

  timestep_number = 0;
  time            = 0;

  /*
  VectorTools::interpolate(dof_handler,
		  	  	  	       ZeroFunction<dim>(dim),
                           old_solution);

*/
  VectorTools::project (dof_handler,
                        constraints,
                        QGauss<dim>(2),
                        ZeroFunction<dim>(dim),//bubble_gum,
                        old_solution);

  solution = old_solution;
  output_results();
//  old_solution = 0;
//  solution     = 0;

   do{

      std::cout << "Time step " << timestep_number << " at t=" << time
                << std::endl;

      assemble_system_2 ();

      solve ();
      output_results ();

      if((timestep_number ==1)&& (pre_refinement_step < n_adaptive_pre_refinement_steps)){

    	  refine_grid(initial_global_refinement,
    			  initial_global_refinement + n_adaptive_pre_refinement_steps);
    	  ++pre_refinement_step;
    	  old_old_solution.reinit(solution.size());
    	  old_solution.reinit(solution.size());
    	  system_rhs.reinit(solution.size());

    	  goto start_time_iteration;

      }
      else if ((timestep_number > 0) && (timestep_number % 5 == 0)){

    	  refine_grid(initial_global_refinement,
    			  initial_global_refinement + n_adaptive_pre_refinement_steps);
    	  old_old_solution.reinit(solution.size());
     	  old_solution.reinit(solution.size());
          system_rhs.reinit(solution.size());
      }
      time += time_step;
      ++timestep_number;

      VectorTools::integrate_difference (dof_handler,
                                         solution,
                                         exact_sol,
                                         difference_per_cell,
                                         QGauss<dim>(3),
                                         VectorTools::L2_norm,
                                         &velocity_mask);
      L2_error = difference_per_cell.l2_norm();
      error_out << time <<"  "<<L2_error << std::endl;

      old_old_solution = old_solution;
      old_solution = solution;
      solution = 0;
  }while (time <= 1.0);


}



int main ()
{

  try
    {
      using namespace dealii;
      deallog.depth_console(0);

      Burger<2> burger_equation_solver;
      burger_equation_solver.run();
    }
  catch (std::exception &exc)
    {
      std::cerr << std::endl << std::endl
                << "----------------------------------------------------"
                << std::endl;
      std::cerr << "Exception on processing: " << std::endl << exc.what()
                << std::endl << "Aborting!" << std::endl
                << "----------------------------------------------------"
                << std::endl;
      return 1;
    }
  catch (...)
    {
      std::cerr << std::endl << std::endl
                << "----------------------------------------------------"
                << std::endl;
      std::cerr << "Unknown exception!" << std::endl << "Aborting!"
                << std::endl
                << "----------------------------------------------------"
                << std::endl;
      return 1;
    }
  return 0;
}
