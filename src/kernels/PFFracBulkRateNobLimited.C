/****************************************************************/
/* MOOSE - Multiphysics Object Oriented Simulation Environment  */
/*                                                              */
/*          All contents are licensed under LGPL V2.1           */
/*             See LICENSE for full restrictions                */
/****************************************************************/
#include "PFFracBulkRateNobLimited.h"
#include "MathUtils.h"

template <>
InputParameters
validParams<PFFracBulkRateNobLimited>()
{
  InputParameters params = validParams<KernelValue>();
  params.addClassDescription(
      "Kernel to compute bulk energy contribution to damage order parameter residual equation");
  params.addRequiredParam<Real>("l", "Interface width");
  params.addRequiredParam<Real>("visco", "Viscosity parameter");
  params.addRequiredParam<MaterialPropertyName>("gc_prop_var",
                                                "Material property name with gc value");
  params.addRequiredParam<MaterialPropertyName>(
      "G0_var", "Material property name with undamaged strain energy driving damage (G0_pos)");
  params.addParam<MaterialPropertyName>(
      "dG0_dstrain_var", "Material property name with derivative of G0_pos with strain");

  params.addCoupledVar("displacements",
                       "The string of displacements suitable for the problem statement");
  params.addParam<std::string>("base_name", "Material property base name");
  params.addRequiredCoupledVar("d2c_dx2","Second derivative of damage with respect to x");
  params.addRequiredCoupledVar("d2c_dy2","Second derivative of damage with respect to y");
  params.addRequiredCoupledVar("d2c_dz2","Second derivative of damage with respect to z");

  return params;
}

PFFracBulkRateNobLimited::PFFracBulkRateNobLimited(const InputParameters & parameters)
  : KernelValue(parameters),
    _gc_prop(getMaterialProperty<Real>("gc_prop_var")),
    _G0_pos(getMaterialProperty<Real>("G0_var")),
    _dG0_pos_dstrain(isParamValid("dG0_dstrain_var")
                         ? &getMaterialProperty<RankTwoTensor>("dG0_dstrain_var")
                         : NULL),
    _ndisp(coupledComponents("displacements")),
    _disp_var(_ndisp),
    _base_name(isParamValid("base_name") ? getParam<std::string>("base_name") + "_" : ""),
    _d2c_dx2(coupledValue("d2c_dx2")),
    _d2c_dy2(coupledValue("d2c_dy2")),
    _d2c_dz2(coupledValue("d2c_dz2")),
    _l(getParam<Real>("l")),
    _visco(getParam<Real>("visco"))
{
  for (unsigned int i = 0; i < _ndisp; ++i)
    _disp_var[i] = coupled("displacements", i);
}

Real
PFFracBulkRateNobLimited::precomputeQpResidual()
{
  const Real gc = _gc_prop[_qp];
  const Real c = _u[_qp];
  const Real beta = _d2c_dx2[_qp]+_d2c_dy2[_qp]+_d2c_dz2[_qp];
  const Real x = _l * beta + 2.0 * (1.0 - c) * _G0_pos[_qp] / gc - c / _l;

  if ( c <= 0.0 || c >= 1.0 ) {
    return 0.0;
  }
  else {
    return -((std::abs(x) + x) / 2.0) / _visco;
  }
}

Real
PFFracBulkRateNobLimited::precomputeQpJacobian()
{
  const Real gc = _gc_prop[_qp];
  const Real c = _u[_qp];
  const Real beta = _d2c_dx2[_qp]+_d2c_dy2[_qp]+_d2c_dz2[_qp];
  const Real x = _l * beta + 2.0 * (1.0 - c) * _G0_pos[_qp] / gc - c / _l;

  if ( c <= 0.0 || c >= 1.0 ) {
    return 0.0;
  }
  else {
    return (MathUtils::sign(x) + 1.0) / 2.0 * (2.0 * _G0_pos[_qp] / gc + 1.0 / _l) / _visco *
           _phi[_j][_qp];
  }
}

Real
PFFracBulkRateNobLimited::computeQpOffDiagJacobian(unsigned int jvar)
{
  unsigned int c_comp;
  bool disp_flag = false;

  const Real c = _u[_qp];
  const Real gc = _gc_prop[_qp];
  const Real beta = _d2c_dx2[_qp]+_d2c_dy2[_qp]+_d2c_dz2[_qp];

  const Real x = _l * beta + 2.0 * (1.0 - c) * (_G0_pos[_qp] / gc) - c / _l;

  const Real signx = MathUtils::sign(x);

  Real xfacbeta = -((signx + 1.0) / 2.0) / _visco * _l;
  Real xfac = -((signx + 1.0) / 2.0) / _visco * 2.0 * (1.0 - c) / gc;

  // Contribution of auxiliary variable to off diag Jacobian of c
  for (unsigned int k = 0; k < _ndisp; ++k)
  {
    if (jvar == _disp_var[k])
    {
      c_comp = k;
      disp_flag = true;
    }
    else
      return 0.0;
  }

  // Contribution of displacements to off diag Jacobian of c
  if (disp_flag && _dG0_pos_dstrain != NULL)
  {
    Real val = 0.0;
    for (unsigned int i = 0; i < LIBMESH_DIM; ++i)
      val += ((*_dG0_pos_dstrain)[_qp](c_comp, i) + (*_dG0_pos_dstrain)[_qp](i, c_comp)) / 2.0 *
             _grad_phi[_j][_qp](i);

    if ( c <= 0.0 || c >= 1.0 ) {
      return 0.0;
    }
    else {
      return xfac * val * _test[_i][_qp];
    }
  }

  return 0.0;
}
