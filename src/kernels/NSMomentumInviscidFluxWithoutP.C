/****************************************************************/
/* MOOSE - Multiphysics Object Oriented Simulation Environment  */
/*                                                              */
/*          All contents are licensed under LGPL V2.1           */
/*             See LICENSE for full restrictions                */
/****************************************************************/

// Navier-Stokes includes
#include "NSMomentumInviscidFluxWithoutP.h"
#include "NS.h"

// FluidProperties includes
#include "IdealGasFluidProperties.h"

template<>
InputParameters validParams<NSMomentumInviscidFluxWithoutP>()
{
  InputParameters params = validParams<NSKernel>();
  // params.addRequiredCoupledVar(NS::pressure, "pressure"); // pressure is not needed
  params.addRequiredParam<unsigned int>("component", "0,1,2 depending on if we are solving the x,y,z component of the momentum equation");
  return params;
}

NSMomentumInviscidFluxWithoutP::NSMomentumInviscidFluxWithoutP(const InputParameters & parameters) :
    NSKernel(parameters),
    // _pressure(coupledValue(NS::pressure)), // pressure is not needed
    _component(getParam<unsigned int>("component"))
{
}

Real
NSMomentumInviscidFluxWithoutP::computeQpResidual()
{
  // For _component = k,

  // (rho*u) * u_k = (rho*u_k) * u <- we write it this way
  RealVectorValue vec(_u[_qp] * _u_vel[_qp],   // (U_k) * u_1
                      _u[_qp] * _v_vel[_qp],   // (U_k) * u_2
                      _u[_qp] * _w_vel[_qp]);  // (U_k) * u_3

  // (rho*u_k) * u + e_k * P [ e_k = unit vector in k-direction ]
  // vec(_component) += _pressure[_qp]; // pressure is not needed

  // -((rho*u_k) * u + e_k * P) * grad(phi)
  return -(vec * _grad_test[_i][_qp]);
}

Real
NSMomentumInviscidFluxWithoutP::computeQpJacobian()
{
  // The on-diagonal entry corresponds to variable number _component+1.
  return computeJacobianHelper(_component + 1);
}

Real
NSMomentumInviscidFluxWithoutP::computeQpOffDiagJacobian(unsigned int jvar)
{
  // Map jvar into the variable m for our problem, regardless of
  // how Moose has numbered things.
  unsigned m = mapVarNumber(jvar);

  return computeJacobianHelper(m);
}

Real
NSMomentumInviscidFluxWithoutP::computeJacobianHelper(unsigned int m)
{
  const RealVectorValue vel(_u_vel[_qp], _v_vel[_qp], _w_vel[_qp]);

  // Ratio of specific heats
  const Real gam = _fp.gamma();

  switch (m)
  {
  case 0: // density
  {
    const Real V2 = vel.norm_sq();
    return vel(_component) * (vel * _grad_test[_i][_qp]) - 0.5 * (gam - 1.0) * V2 * _grad_test[_i][_qp](_component);
  }

  case 1:
  case 2:
  case 3: // momentums
  {
    // Map m into m_local = {0,1,2}
    unsigned int m_local = m - 1;

    // Kronecker delta
    const Real delta_kl = (_component == m_local ? 1. : 0.);

    return -1.0 * (vel(_component) * _grad_test[_i][_qp](m_local)
                   + delta_kl * (vel * _grad_test[_i][_qp])
                   + (1. - gam) * vel(m_local) * _grad_test[_i][_qp](_component)) * _phi[_j][_qp];
  }

  case 4: // energy
    return -1.0 * (gam - 1.0) * _phi[_j][_qp] * _grad_test[_i][_qp](_component);
  }

  mooseError("Shouldn't get here!");
}
