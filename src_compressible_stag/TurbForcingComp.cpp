#include "TurbForcingComp.H"

// initialize n_rngs, geom
// build MultiFabs to hold random numbers
TurbForcingComp::TurbForcingComp()
{}

void TurbForcingComp::define(BoxArray ba_in, DistributionMapping dmap_in,
                    const Real& a_in, const Real& b_in, const Real& c_in, const Real& d_in, const Real& alpha_in)
{
    BL_PROFILE_VAR("TurbForcingComp::define()",TurbForcingCompDefine);

    ForcingS.resize(132);
    ForcingC.resize(132);
    ForcingSold.resize(132);
    ForcingCold.resize(132);
    Real* const AMREX_RESTRICT forcing_S = GetForcingS();
    Real* const AMREX_RESTRICT forcing_C = GetForcingC();
    Real* const AMREX_RESTRICT forcing_Sold = GetForcingSOld();
    Real* const AMREX_RESTRICT forcing_Cold = GetForcingCOld();
    amrex::ParallelFor(132, [forcing_S, forcing_C, forcing_Sold, forcing_Cold]
    AMREX_GPU_DEVICE (int i) noexcept
    {
        forcing_S[i] = 0.;
        forcing_C[i] = 0.;
        forcing_Sold[i] = 0.;
        forcing_Cold[i] = 0.;
    });
    
    forcing_a = a_in;
    forcing_b = b_in;
    forcing_c = c_in;
    forcing_d = d_in;
    alpha     = alpha_in;
    
    for (int i=0; i<AMREX_SPACEDIM; ++i) {
        sines  [i].define(convert(ba_in,nodal_flag_dir[i]), dmap_in, 22, 0);
        cosines[i].define(convert(ba_in,nodal_flag_dir[i]), dmap_in, 22, 0);
    }
}

void TurbForcingComp::Initialize(const Geometry& geom_in) {

    Real L = prob_hi[0] - prob_lo[0];

    if (prob_hi[0] - prob_lo[0] !=  prob_hi[1] - prob_lo[1]) {
        Abort("TurbForce requires square domain for now");
    }
#if (AMREX_SPACEDIM == 3)
    if (prob_hi[0] - prob_lo[0] !=  prob_hi[2] - prob_lo[2]) {
        Abort("TurbForce requires square domain for now");
    }
#endif

    const GpuArray<Real,AMREX_SPACEDIM> dx = geom_in.CellSizeArray();

    GpuArray<Real,AMREX_SPACEDIM> prob_lo_gpu = geom_in.ProbLoArray();
    
    int* const AMREX_RESTRICT kx = GetKX();
    int* const AMREX_RESTRICT ky = GetKY();
    int* const AMREX_RESTRICT kz = GetKZ();
    
    // Loop over boxes
    for (MFIter mfi(sines[0],TilingIfNotGPU()); mfi.isValid(); ++mfi) {

        AMREX_D_TERM(const Array4<Real> & sin_x = sines[0].array(mfi);,
                     const Array4<Real> & sin_y = sines[1].array(mfi);,
                     const Array4<Real> & sin_z = sines[2].array(mfi););
        
        AMREX_D_TERM(const Array4<Real> & cos_x = cosines[0].array(mfi);,
                     const Array4<Real> & cos_y = cosines[1].array(mfi);,
                     const Array4<Real> & cos_z = cosines[2].array(mfi););

        // since the MFIter is built on a nodal MultiFab we need to build the
        // nodal tileboxes for each direction in this way
        AMREX_D_TERM(Box bx_x = mfi.tilebox(nodal_flag_x);,
                     Box bx_y = mfi.tilebox(nodal_flag_y);,
                     Box bx_z = mfi.tilebox(nodal_flag_z););
    
#if (AMREX_SPACEDIM == 2)
        amrex::ParallelFor(bx_x, bx_y, [=] AMREX_GPU_DEVICE (int i, int j, int k) {
                Real x = prob_lo_gpu[0] + i*dx[0];
                Real y = prob_lo_gpu[1] + (j+0.5)*dx[1];
                for (int d=0; d<22; ++d) {
                    sin_x(i,j,k,d) = std::sin(2.*pi*(kx[d]*x + ky[d]*y) / L);
                    cos_x(i,j,k,d) = std::cos(2.*pi*(kx[d]*x + ky[d]*y) / L);
                }
            },
            [=] AMREX_GPU_DEVICE (int i, int j, int k) {
                Real x = prob_lo_gpu[0] + (i+0.5)*dx[0];
                Real y = prob_lo_gpu[1] + j*dx[1];
                for (int d=0; d<22; ++d) {
                    sin_y(i,j,k,d) = std::sin(2.*pi*(kx[d]*x + ky[d]*y) / L);
                    cos_y(i,j,k,d) = std::cos(2.*pi*(kx[d]*x + ky[d]*y) / L);
                }
            });
#elif (AMREX_SPACEDIM ==3)
        amrex::ParallelFor(bx_x, bx_y, bx_z, [=] AMREX_GPU_DEVICE (int i, int j, int k) {
                Real pi = 3.1415926535897932;
                Real x = prob_lo_gpu[0] + i*dx[0];
                Real y = prob_lo_gpu[1] + (j+0.5)*dx[1];
                Real z = prob_lo_gpu[2] + (k+0.5)*dx[2];
                for (int d=0; d<22; ++d) {
                    sin_x(i,j,k,d) = std::sin(2.*pi*(kx[d]*x + ky[d]*y + kz[d]*z) / L);
                    cos_x(i,j,k,d) = std::cos(2.*pi*(kx[d]*x + ky[d]*y + kz[d]*z) / L);
                }
            },
            [=] AMREX_GPU_DEVICE (int i, int j, int k) {
                Real pi = 3.1415926535897932;
                Real x = prob_lo_gpu[0] + (i+0.5)*dx[0];
                Real y = prob_lo_gpu[1] + j*dx[1];
                Real z = prob_lo_gpu[2] + (k+0.5)*dx[2];
                for (int d=0; d<22; ++d) {
                    sin_y(i,j,k,d) = std::sin(2.*pi*(kx[d]*x + ky[d]*y + kz[d]*z) / L);
                    cos_y(i,j,k,d) = std::cos(2.*pi*(kx[d]*x + ky[d]*y + kz[d]*z) / L);
                }
            },
            [=] AMREX_GPU_DEVICE (int i, int j, int k) {
                Real pi = 3.1415926535897932;
                Real x = prob_lo_gpu[0] + (i+0.5)*dx[0];
                Real y = prob_lo_gpu[1] + (j+0.5)*dx[1];
                Real z = prob_lo_gpu[2] + k*dx[2];
                for (int d=0; d<22; ++d) {
                    sin_z(i,j,k,d) = std::sin(2.*pi*(kx[d]*x + ky[d]*y + kz[d]*z) / L);
                    cos_z(i,j,k,d) = std::cos(2.*pi*(kx[d]*x + ky[d]*y + kz[d]*z) / L);
                }
            });
#endif
    }
    
}

void TurbForcingComp::CalcTurbForcingComp(std::array< MultiFab, AMREX_SPACEDIM >& vel_f,
                                 const Real& dt,
                                 const int& update)
{

    Real sqrtdt = std::sqrt(dt);
    
    Real* const AMREX_RESTRICT forcing_S = GetForcingS();
    Real* const AMREX_RESTRICT forcing_C = GetForcingC();
    
    Real* const AMREX_RESTRICT forcing_Sold = GetForcingSOld();
    Real* const AMREX_RESTRICT forcing_Cold = GetForcingCOld();

    // update U = U - a*dt + b*sqrt(dt)*Z
    if (update == 1) {
        Gpu::DeviceVector<Real> rngs_s(132); // solenoidal
        Gpu::DeviceVector<Real> rngs_c(132); // comopressional
        Real* const AMREX_RESTRICT rngs_s_gpu = rngs_s.dataPtr();
        Real* const AMREX_RESTRICT rngs_c_gpu = rngs_c.dataPtr();

        amrex::ParallelForRNG(132, [rngs_s_gpu,rngs_c_gpu]
        AMREX_GPU_DEVICE (int i, amrex::RandomEngine const& engine) noexcept
        {
            rngs_s_gpu[i] = amrex::RandomNormal(0.,1.,engine);
            rngs_c_gpu[i] = amrex::RandomNormal(0.,1.,engine);
        });

        // update forcing (OU)
        Real forcing_a_gpu = forcing_a;
        Real forcing_b_gpu = forcing_b;
        Real forcing_c_gpu = forcing_c;
        Real forcing_d_gpu = forcing_d;
        Real dt_gpu = dt;
        Real sqrtdt_gpu = sqrtdt;
        amrex::ParallelFor(132, [forcing_S, forcing_C, forcing_Sold, forcing_Cold, rngs_s_gpu, rngs_c_gpu, 
                                 forcing_a_gpu, forcing_b_gpu, forcing_c_gpu, forcing_d_gpu, dt_gpu, sqrtdt_gpu]
        AMREX_GPU_DEVICE (int i) noexcept
        {
            forcing_S[i] = forcing_Sold[i] - forcing_a_gpu*forcing_Sold[i]*dt_gpu + forcing_b_gpu*sqrtdt_gpu*rngs_s_gpu[i];
            forcing_C[i] = forcing_Cold[i] - forcing_c_gpu*forcing_Cold[i]*dt_gpu + forcing_d_gpu*sqrtdt_gpu*rngs_c_gpu[i];
        });

        copy_new_forcing_to_old();
    }

    int* const AMREX_RESTRICT Kx = GetKX();
    int* const AMREX_RESTRICT Ky = GetKY();
    int* const AMREX_RESTRICT Kz = GetKZ();

    Real alpha_gpu = alpha;

    // Loop over boxes
    for (MFIter mfi(sines[0],TilingIfNotGPU()); mfi.isValid(); ++mfi) {

        AMREX_D_TERM(const Array4<Real> & sin_x = sines[0].array(mfi);,
                     const Array4<Real> & sin_y = sines[1].array(mfi);,
                     const Array4<Real> & sin_z = sines[2].array(mfi););
        
        AMREX_D_TERM(const Array4<Real> & cos_x = cosines[0].array(mfi);,
                     const Array4<Real> & cos_y = cosines[1].array(mfi);,
                     const Array4<Real> & cos_z = cosines[2].array(mfi););
        
        AMREX_D_TERM(const Array4<Real> & vel_x = vel_f[0].array(mfi);,
                     const Array4<Real> & vel_y = vel_f[1].array(mfi);,
                     const Array4<Real> & vel_z = vel_f[2].array(mfi););

        // since the MFIter is built on a nodal MultiFab we need to build the
        // nodal tileboxes for each direction in this way
        AMREX_D_TERM(Box bx_x = mfi.tilebox(nodal_flag_x);,
                     Box bx_y = mfi.tilebox(nodal_flag_y);,
                     Box bx_z = mfi.tilebox(nodal_flag_z););
    
#if (AMREX_SPACEDIM == 2)
        Warning("2D CalcTurbForcingComp not defined yet");
        amrex::ParallelFor(bx_x, bx_y, [=] AMREX_GPU_DEVICE (int i, int j, int k) {
            },
            [=] AMREX_GPU_DEVICE (int i, int j, int k) {
            }
            );
#elif (AMREX_SPACEDIM ==3)
        amrex::ParallelFor(bx_x, bx_y, bx_z, [=] AMREX_GPU_DEVICE (int i, int j, int k) {
                for (int d=0; d<22; ++d) {
                    Real kx = Real(Kx[d]);
                    Real ky = Real(Ky[d]);
                    Real kz = Real(Kz[d]);
                    Real kk = kx*kx + ky*ky + kz*kz;
                    Real forcingScos = alpha_gpu*cos_x(i,j,k,d)*(forcing_S[d]*(1.0-kx*kx/kk)    - forcing_S[d+22]*(kx*ky/kk) - forcing_S[d+44]*(kx*kz/kk)); // solenoidal;
                    Real forcingSsin = alpha_gpu*sin_x(i,j,k,d)*(forcing_S[d+66]*(1.0-kx*kx/kk) - forcing_S[d+88]*(kx*ky/kk) - forcing_S[d+110]*(kx*kz/kk)); // solenoidal;
                    vel_x(i,j,k)    += forcingScos + forcingSsin;
                    Real forcingCcos = (1.0-alpha_gpu)*cos_x(i,j,k,d)*(forcing_C[d]*(kx*kx/kk)    + forcing_C[d+22]*(kx*ky/kk) + forcing_C[d+44]*(kx*kz/kk)); // compressional
                    Real forcingCsin = (1.0-alpha_gpu)*sin_x(i,j,k,d)*(forcing_C[d+66]*(kx*kx/kk) + forcing_C[d+88]*(kx*ky/kk) + forcing_C[d+110]*(kx*kz/kk)); // compressional
                    vel_x(i,j,k)    += forcingCcos + forcingCsin;
                }
            },
            [=] AMREX_GPU_DEVICE (int i, int j, int k) {
                for (int d=0; d<22; ++d) {
                    Real kx = Real(Kx[d]);
                    Real ky = Real(Ky[d]);
                    Real kz = Real(Kz[d]);
                    Real kk = kx*kx + ky*ky + kz*kz;
                    Real forcingScos = alpha_gpu*cos_y(i,j,k,d)*(-forcing_S[d]*(kx*ky/kk)    + forcing_S[d+22]*(1.0-ky*ky/kk) - forcing_S[d+44]*(ky*kz/kk)); // solenoidal;
                    Real forcingSsin = alpha_gpu*sin_y(i,j,k,d)*(-forcing_S[d+66]*(kx*ky/kk) + forcing_S[d+88]*(1.0-ky*ky/kk) - forcing_S[d+110]*(ky*kz/kk)); // solenoidal;
                    vel_y(i,j,k)    += forcingScos + forcingSsin;
                    Real forcingCcos = (1.0-alpha_gpu)*cos_y(i,j,k,d)*(forcing_C[d]*(kx*ky/kk)    + forcing_C[d+22]*(ky*ky/kk) + forcing_C[d+44]*(ky*kz/kk)); // compressional
                    Real forcingCsin = (1.0-alpha_gpu)*sin_y(i,j,k,d)*(forcing_C[d+66]*(kx*ky/kk) + forcing_C[d+88]*(ky*ky/kk) + forcing_C[d+110]*(ky*kz/kk)); // compressional
                    vel_y(i,j,k)    += forcingCcos + forcingCsin;
                }
                
            },
            [=] AMREX_GPU_DEVICE (int i, int j, int k) {
                for (int d=0; d<22; ++d) {
                    Real kx = Real(Kx[d]);
                    Real ky = Real(Ky[d]);
                    Real kz = Real(Kz[d]);
                    Real kk = kx*kx + ky*ky + kz*kz;
                    Real forcingScos = alpha_gpu*cos_z(i,j,k,d)*(-forcing_S[d]*(kx*kz/kk)    - forcing_S[d+22]*(ky*kz/kk) + forcing_S[d+44]*(1.0-kz*kz/kk)); // solenoidal;
                    Real forcingSsin = alpha_gpu*sin_z(i,j,k,d)*(-forcing_S[d+66]*(kx*kz/kk) - forcing_S[d+88]*(ky*kz/kk) + forcing_S[d+110]*(1.0-kz*kz/kk)); // solenoidal;
                    vel_z(i,j,k)    += forcingScos + forcingSsin;
                    Real forcingCcos = (1.0-alpha_gpu)*cos_z(i,j,k,d)*(forcing_C[d]*(kx*kz/kk)    + forcing_C[d+22]*(ky*kz/kk) + forcing_C[d+44]*(kz*kz/kk)); // compressional
                    Real forcingCsin = (1.0-alpha_gpu)*sin_z(i,j,k,d)*(forcing_C[d+66]*(kx*kz/kk) + forcing_C[d+88]*(ky*kz/kk) + forcing_C[d+110]*(kz*kz/kk)); // compressional
                    vel_z(i,j,k)    += forcingCcos + forcingCsin;
                }                
            });
#endif
    }    
}

std::tuple<amrex::Real, Real> TurbForcingComp::getU(const int& i) {
    return {ForcingS[i], ForcingC[i]};
}

void TurbForcingComp::setU(const int& i, Real fs, Real fc) {
    ForcingS[i] = fs;
    ForcingC[i] = fc;
}

void TurbForcingComp::copy_new_forcing_to_old()
{
    Real* const AMREX_RESTRICT forcing_S = GetForcingS();
    Real* const AMREX_RESTRICT forcing_C = GetForcingC();
    
    Real* const AMREX_RESTRICT forcing_Sold = GetForcingSOld();
    Real* const AMREX_RESTRICT forcing_Cold = GetForcingCOld();
        
    amrex::ParallelFor(132, [forcing_S, forcing_C, forcing_Sold, forcing_Cold]
    AMREX_GPU_DEVICE (int i) noexcept
    {
        forcing_Sold[i] = forcing_S[i];
        forcing_Cold[i] = forcing_C[i];
    });
}
