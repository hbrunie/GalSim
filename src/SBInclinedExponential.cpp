/* -*- c++ -*-
 * Copyright (c) 2012-2016 by the GalSim developers team on GitHub
 * https://github.com/GalSim-developers
 *
 * This file is part of GalSim: The modular galaxy image simulation toolkit.
 * https://github.com/GalSim-developers/GalSim
 *
 * GalSim is free software: redistribution and use in source and binary forms,
 * with or without modification, are permitted provided that the following
 * conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions, and the disclaimer given in the accompanying LICENSE
 *    file.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions, and the disclaimer given in the documentation
 *    and/or other materials provided with the distribution.
 */

//#define DEBUGLOGGING

#include "galsim/IgnoreWarnings.h"

#define BOOST_NO_CXX11_SMART_PTR

#include "SBInclinedExponential.h"
#include "SBInclinedExponentialImpl.h"
#include "integ/Int.h"
#include "Solve.h"

#ifdef DEBUGLOGGING
#include <fstream>
// std::ostream* dbgout = new std::ofstream("debug.out");
std::ostream* dbgout = &std::cout;
int verbose_level = 2;
#endif

namespace galsim {

    SBInclinedExponential::SBInclinedExponential(Angle inclination, double scale_radius, double scale_height, double flux,
            const GSParamsPtr& gsparams) :
        SBProfile(new SBInclinedExponentialImpl(inclination, scale_radius, scale_height, flux, gsparams)) {}

    SBInclinedExponential::SBInclinedExponential(const SBInclinedExponential& rhs) : SBProfile(rhs) {}

    SBInclinedExponential::~SBInclinedExponential() {}

    Angle SBInclinedExponential::getInclination() const
    {
        assert(dynamic_cast<const SBInclinedExponentialImpl*>(_pimpl.get()));
        return static_cast<const SBInclinedExponentialImpl&>(*_pimpl).getInclination();
    }

    double SBInclinedExponential::getScaleRadius() const
    {
        assert(dynamic_cast<const SBInclinedExponentialImpl*>(_pimpl.get()));
        return static_cast<const SBInclinedExponentialImpl&>(*_pimpl).getScaleRadius();
    }

    double SBInclinedExponential::getScaleHeight() const
    {
        assert(dynamic_cast<const SBInclinedExponentialImpl*>(_pimpl.get()));
        return static_cast<const SBInclinedExponentialImpl&>(*_pimpl).getScaleHeight();
    }

    // NB.  This function is virtually wrapped by repr() in SBProfile.cpp
    std::string SBInclinedExponential::SBInclinedExponentialImpl::serialize() const
    {
        std::ostringstream oss(" ");
        // NB. The choice of digits10 + 4 is because the normal general output
        // scheme for double uses fixed notation if >= 0.0001, but then switches
        // to scientific for smaller numbers.  So those first 4 digits in 0.0001 don't
        // count for the number of required digits, which is nominally given by digits10.
        // cf. http://stackoverflow.com/questions/4738768/printing-double-without-losing-precision
        // Unfortunately, there doesn't seem to be an easy equivalent of python's %r for
        // printing the repr of a double that always works and doesn't print more digits than
        // it needs.  This is the reason why we reimplement the __repr__ methods in python
        // for all the SB classes except SBProfile.  Only the last one can't be done properly
        // in python, so it will use the C++ virtual function to get the right thing for
        // any subclass.  But possibly with ugly extra digits.
        oss.precision(std::numeric_limits<double>::digits10 + 4);
        oss << "galsim._galsim.SBInclinedExponential("<<getInclination()<<", "<<getScaleRadius()<<", "<<getScaleHeight();
        oss <<", "<<getFlux()<<", False";
        oss << ", galsim.GSParams("<<*gsparams<<"))";
        return oss.str();
    }

    SBInclinedExponential::SBInclinedExponentialImpl::SBInclinedExponentialImpl(Angle inclination, double scale_radius,
            double scale_height, double flux, const GSParamsPtr& gsparams) :
        SBProfileImpl(gsparams),
        _inclination(inclination),
        _r0(scale_radius),
        _h0(scale_height),
        _flux(flux),
        _inv_r0(1./_r0)
    {
        dbg<<"Start SBInclinedExponential constructor:\n";
        dbg<<"inclination = "<<_inclination<<std::endl;
        dbg<<"scale radius = "<<_r0<<std::endl;
        dbg<<"scale height = "<<_h0<<std::endl;
        dbg<<"flux = "<<_flux<<std::endl;

        // Now set up, using this value of cosi

        _half_pi_h_sini_over_r = 0.5*M_PI*scale_height*std::abs(inclination.sin())*_inv_r0;

        _cosi = std::abs(inclination.cos());

        double cosi_squared = _cosi*_cosi;

        xdbg<<"_half_pi_h_sini_over_r = "<<_half_pi_h_sini_over_r<<std::endl;
        xdbg<<"_cosi = "<<_cosi<<std::endl;

        // For large k, we clip the result of kValue to 0.
        // We do this when the correct answer is less than kvalue_accuracy.
        // (1+k^2 r0^2 cos^2(i))^-1.5 = kvalue_accuracy
        // The convolution factor is generally a small effect, so we don't include it here
        _ksq_max = (std::pow(this->gsparams->kvalue_accuracy,-1./1.5) - 1.)/cosi_squared;

        // For small k, we can use up to quartic in the taylor expansion to avoid the sqrt.
        // This is acceptable when the next term is less than kvalue_accuracy.
        // 35/16 (k^2 r0^2)^3 = kvalue_accuracy
        _ksq_min = std::pow(this->gsparams->kvalue_accuracy * 16./35., 1./3.);

        // Calculate stepk, based on a conservative comparison to an exponential disk. The
        // half-light radius of this will be smaller, so if we use an exponential's hlr, it
        // will be at least large enough.

        // int( exp(-r) r, r=0..R) = (1 - exp(-R) - Rexp(-R))
        // Fraction excluded is thus (1+R) exp(-R)
        // A fast solution to (1+R)exp(-R) = x:
        // log(1+R) - R = log(x)
        // R = log(1+R) - log(x)
        double logx = std::log(this->gsparams->folding_threshold);
        double R = -logx;
        for (int i=0; i<3; i++) R = std::log(1.+R) - logx;
        // Make sure it is at least 5 hlr of corresponding exponential
        // half-light radius = 1.6783469900166605 * r0
        const double exp_hlr = 1.6783469900166605;
        R = std::max(R,this->gsparams->stepk_minimum_hlr*exp_hlr);
        _stepk = M_PI / R;
        dbg<<"stepk = "<<_stepk<<std::endl;

        // Solve for the proper _maxk

        double maxk_min = std::pow(this->gsparams->maxk_threshold, -1./3.);

        // Check for face-on case, which doesn't need the solver
        if(_cosi==1)
        {
        	_maxk = maxk_min;
        }
        else // Use the solver
        {
        	// Bracket it appropriately, starting with guesses based on the 1/cosi scaling
			double maxk_max;
			// Check bounds on _cosi to make sure initial guess range isn't too big or small
			if(_cosi>0.01)
			{
				if(_cosi<0.96)
					maxk_max = maxk_min/_cosi;
				else
					maxk_max = 1.05*maxk_min;
			}
			else
			{
				maxk_max = 100*maxk_min;
			}

			xdbg << "maxk_threshold = " << this->gsparams->maxk_threshold << std::endl;
			xdbg << "F(" << maxk_min << ") = " << kValueHelper(0.,maxk_min) << std::endl;
			xdbg << "F(" << maxk_max << ") = " << kValueHelper(0.,maxk_max) << std::endl;

			SBInclinedExponentialMaxKFunctor func(this);
			Solve<SBInclinedExponentialMaxKFunctor> solver(func, maxk_min, maxk_max);
			solver.bracket();

			// Get the _maxk from the solver here. We add back on the tolerance to the result to
			// ensure that the k-value will be below the threshold.
			_maxk = solver.root() + solver.getXTolerance();
        }
    }

    double SBInclinedExponential::SBInclinedExponentialImpl::xValue(const Position<double>& p) const
    {
        throw std::runtime_error("Real-space expression of SBInclinedExponential NYI.");
        return 0;
    }

    std::complex<double> SBInclinedExponential::SBInclinedExponentialImpl::kValue(const Position<double>& k) const
    {
        double kx = k.x*_r0;
        double ky = k.y*_r0;
        return _flux * kValueHelper(kx,ky);
    }

    void SBInclinedExponential::SBInclinedExponentialImpl::fillKValue(tmv::MatrixView<std::complex<double> > val,
                                            double kx0, double dkx, int izero,
                                            double ky0, double dky, int jzero) const
    {
        dbg<<"SBInclinedExponential fillKValue\n";
        dbg<<"kx = "<<kx0<<" + i * "<<dkx<<", izero = "<<izero<<std::endl;
        dbg<<"ky = "<<ky0<<" + j * "<<dky<<", jzero = "<<jzero<<std::endl;
        if (izero != 0 || jzero != 0) {
            xdbg<<"Use Quadrant\n";
            fillKValueQuadrant(val,kx0,dkx,izero,ky0,dky,jzero);
        } else {
            xdbg<<"Non-Quadrant\n";
            assert(val.stepi() == 1);
            const int m = val.colsize();
            const int n = val.rowsize();
            typedef tmv::VIt<std::complex<double>,1,tmv::NonConj> It;

            kx0 *= _r0;
            dkx *= _r0;
            ky0 *= _r0;
            dky *= _r0;

            for (int j=0;j<n;++j,ky0+=dky) {
                double kx = kx0;
                It valit = val.col(j).begin();
                for (int i=0;i<m;++i,kx+=dkx) {
                    double new_val = _flux * kValueHelper(kx,ky0);
                    xxdbg << "kx = " << kx << "\tky = " << ky0 << "\tval = " << new_val << std::endl;
                    *valit++ = new_val;
                }
            }
        }
    }

    void SBInclinedExponential::SBInclinedExponentialImpl::fillKValue(tmv::MatrixView<std::complex<double> > val,
                                            double kx0, double dkx, double dkxy,
                                            double ky0, double dky, double dkyx) const
    {
        dbg<<"SBInclinedExponential fillKValue\n";
        dbg<<"kx = "<<kx0<<" + i * "<<dkx<<" + j * "<<dkxy<<std::endl;
        dbg<<"ky = "<<ky0<<" + i * "<<dkyx<<" + j * "<<dky<<std::endl;
        assert(val.stepi() == 1);
        assert(val.canLinearize());
        const int m = val.colsize();
        const int n = val.rowsize();
        typedef tmv::VIt<std::complex<double>,1,tmv::NonConj> It;

        kx0 *= _r0;
        dkx *= _r0;
        dkxy *= _r0;
        ky0 *= _r0;
        dky *= _r0;
        dkyx *= _r0;

        It valit = val.linearView().begin();
        for (int j=0;j<n;++j,kx0+=dkxy,ky0+=dky) {
            double kx = kx0;
            double ky = ky0;
            for (int i=0;i<m;++i,kx+=dkx,ky+=dkyx) {
                *valit++ = _flux * kValueHelper(kx,ky);
            }
        }
    }

    double SBInclinedExponential::SBInclinedExponentialImpl::maxK() const { return _maxk * _inv_r0; }
    double SBInclinedExponential::SBInclinedExponentialImpl::stepK() const { return _stepk * _inv_r0; }

    double SBInclinedExponential::SBInclinedExponentialImpl::kValueHelper(double kx, double ky) const
    {
        // Calculate the base value for an exponential profile

    	double ky_cosi = ky*_cosi;

        double ky_cosi_sq = ky_cosi*ky_cosi;
        double ksq = kx*kx + ky_cosi_sq;
        double res_base;
        if (ksq > _ksq_max)
        {
            return 0.;
        }
        else if (ksq < _ksq_min)
        {
            res_base = (1. - 1.5*ksq*(1. - 1.25*ksq));

            xxdbg << "res_base (upper limit) = " << res_base << std::endl;
        }
        else
        {
            double temp = 1. + ksq;
            res_base =  1./(temp*sqrt(temp));

            xxdbg << "res_base (normal) = " << res_base << std::endl;
        }

        // Calculate the convolution factor
        double res_conv;

        double scaled_ky = _half_pi_h_sini_over_r*ky;
        double scaled_ky_squared = scaled_ky*scaled_ky;

        if (scaled_ky_squared < _ksq_min)
        {
            // Use Taylor expansion to speed up calculation
            res_conv = (1. - 0.16666666667*scaled_ky_squared*(1. - 0.116666666667*scaled_ky_squared));
            xxdbg << "res_conv (lower limit) = " << res_conv << std::endl;
        }
        else
        {
            res_conv = scaled_ky / std::sinh(scaled_ky);
            xxdbg << "res_conv (normal) = " << res_conv << std::endl;
        }


        double res = res_base*res_conv;

        return res;
    }

    // NYI, but needs to be defined
    boost::shared_ptr<PhotonArray> SBInclinedExponential::SBInclinedExponentialImpl::shoot(int N, UniformDeviate ud) const
    {
        throw std::runtime_error("Photon shooting NYI for InclinedExponential profile.");
    }

    SBInclinedExponential::SBInclinedExponentialImpl::SBInclinedExponentialMaxKFunctor::SBInclinedExponentialMaxKFunctor(
    		const SBInclinedExponential::SBInclinedExponentialImpl * pimpl) : _pimpl(pimpl) {}

    double SBInclinedExponential::SBInclinedExponentialImpl::SBInclinedExponentialMaxKFunctor::operator()(double k) const
    {
    	assert(_pimpl);
    	double k_value = std::max(_pimpl->kValueHelper(0.,k),_pimpl->kValueHelper(k,0.));
    	return k_value - _pimpl->gsparams->maxk_threshold;
    }


}
