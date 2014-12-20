/* -*- c++ -*-
 * Copyright (c) 2012-2014 by the GalSim developers team on GitHub
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

#ifndef GalSim_SBSpergel_H
#define GalSim_SBSpergel_H
/**
 * @file SBSpergel.h @brief SBProfile that implements a 2-d Spergel profile.
 */

#include "SBProfile.h"

namespace galsim {

    namespace sbp {

        // Constrain range of allowed Spergel index nu
        // These ranges taken from Table 1 of Spergel (2010), but could be extended with further
        // testing.
        const double minimum_spergel_nu = -0.9;
        const double maximum_spergel_nu = 0.85;

        // How many Spergel profiles to save in the cache
        const int max_spergel_cache = 100;

    }

    /**
     * @brief Spergel Surface Brightness Profile.
     *
     * Surface brightness profile with `I(r) propto (r/r_c)^{nu} K_{nu}(r/r_c)` for some
     * scale-length `r_c = r_0 / c_{nu}`, where `K_{nu}(u)` is the modified Bessel function of the
     * second kind (also confusingly referred to as the 'spherical modified Bessel function of the
     * third kind') and `nu > -1`. For different parameters `nu` this profile can approximate Sersic
     * profiles with different indices.
     *
     * Reference:
     *   D. N. Spergel, "ANALYTICAL GALAXY PROFILES FOR PHOTOMETRIC AND LENSING ANALYSIS,""
     *   ASTROPHYS J SUPPL S 191(1), 58-65 (2010) [doi:10.1088/0067-0049/191/1/58].
     */
    class SBSpergel : public SBProfile
    {
    public:
        enum  RadiusType
        {
            HALF_LIGHT_RADIUS,
            SCALE_RADIUS
        };
        /**
         * @brief Constructor - note that `r0` is scale length, NOT half-light radius `re` as in
         * SBSersic.
         *
         * @param[in] nu       index parameter setting the logarithmic slope of the profile.
         * @param[in] size     Size specification.
         * @param[in] rType    Kind of size being specified (HALF_LIGHT_RADIUS or
         *                     SCALE_RADIUS).
         * @param[in] flux     flux.
         * @param[in] gsparams GSParams object storing constants that control the accuracy of image
         *                     operations and rendering, if different from the default.
         */
        SBSpergel(double nu, double size, RadiusType rType, double flux,
                  const GSParamsPtr& gsparams);

        /// @brief Copy constructor.
        SBSpergel(const SBSpergel& rhs);

        /// @brief Destructor.
        ~SBSpergel();

        /// @brief Returns the Spergel index `nu` of the profile.
        double getNu() const;

        /// @brief Returns the scale radius of the Spergel profile.
        double getScaleRadius() const;

        /// @brief Returns the half light radius of the Spergel profile.
        double getHalfLightRadius() const;

    protected:

        class SBSpergelImpl;

    private:
        // op= is undefined
        void operator=(const SBSpergel& rhs);
    };

}

#endif