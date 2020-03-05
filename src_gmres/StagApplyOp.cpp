
#include "common_functions.H"
#include "common_functions_F.H"

#include "common_namespace.H"

#include "gmres_functions.H"

#include "gmres_namespace.H"

using namespace common;

//Takes cell centred and nodal viscosity multifabs, and face centred velocity
//multifab, and outputs to face-centered velocity multifab.

AMREX_GPU_HOST_DEVICE
inline
void stag_applyop_visc_p1 (Box const& tbx,
			   AMREX_D_DECL(Box const& xbx,
					Box const& ybx,
					Box const& zbx),
                           AMREX_D_DECL(Array4<Real const> const& alphax,
					Array4<Real const> const& alphay,
					Array4<Real const> const& alphaz),
                           AMREX_D_DECL(Array4<Real const> const& phix,
					Array4<Real const> const& phiy,
					Array4<Real const> const& phiz),
                           AMREX_D_DECL(Array4<Real> const& Lphix,
					Array4<Real> const& Lphiy,
					Array4<Real> const& Lphiz),
			   AMREX_D_DECL(bool do_x,
					bool do_y,
					bool do_z),
			   Real theta_alpha, Real bt,  Real gt, int offset,  int color,
			   const GpuArray<Real, AMREX_SPACEDIM> & dx) noexcept
{
    // xbx, ybx, and zbx are the face-centered boxes

    // if running on the host
    // tlo is the minimal box containins the union of the face-centered grid boxes

    // if running on the gpu, tlo is a box with a single point that comes
    // from the union of the face-centered grid boxes

    const auto tlo = lbound(tbx);
    const auto thi = ubound(tbx);

    // if running on the host, x/y/zlo and x/y/zhi are set to
    // the lower/uppser bounds of x/y/zbx

    // if running on the gpu, x/y/zlo and x/y/zhi are set to
    // the single point defined by tlo, unless tlo is outside of the union
    // of the face-centered grid boxes, in which case they are set to
    // values that make sure the loop is not entered

    AMREX_D_TERM(const auto xlo = amrex::elemwiseMax(tlo, lbound(xbx));,
                 const auto ylo = amrex::elemwiseMax(tlo, lbound(ybx));,
                 const auto zlo = amrex::elemwiseMax(tlo, lbound(zbx)););

    AMREX_D_TERM(const auto xhi = amrex::elemwiseMin(thi, ubound(xbx));,
                 const auto yhi = amrex::elemwiseMin(thi, ubound(ybx));,
                 const auto zhi = amrex::elemwiseMin(thi, ubound(zbx)););

    int ioff;

    Real dxsqinv = 1./(dx[0]*dx[0]);
    Real dysqinv = 1./(dx[1]*dx[1]);
#if (AMREX_SPACEDIM == 3)
    Real dzsqinv = 1./(dx[2]*dx[2]);
#endif

#if (AMREX_SPACEDIM == 2)
    Real term1 = 2.*bt*(dxsqinv+dysqinv);
#elif (AMREX_SPACEDIM == 3)
    Real term1 = 2.*bt*(dxsqinv+dysqinv+dzsqinv);
#endif

    Real term2 = bt*dxsqinv;
    Real term3 = bt*dysqinv;

#if (AMREX_SPACEDIM == 3)
    Real term4 = bt*dzsqinv;
#endif

    if (do_x) {

        for (int k = xlo.z; k <= xhi.z; ++k) {
        for (int j = xlo.y; j <= xhi.y; ++j) {
        ioff = 0;
	if (offset == 2 && (xlo.x+j+k)%2 != (color+1)%2 ) {
	  ioff = 1;
	}
        AMREX_PRAGMA_SIMD
        for (int i = xlo.x+ioff; i <= xhi.x; i+=offset) {

            Lphix(i,j,k) = phix(i,j,k)*(theta_alpha*alphax(i,j,k) + term1)
                -(phix(i+1,j,k)+phix(i-1,j,k))*term2
                -(phix(i,j+1,k)+phix(i,j-1,k))*term3
#if (AMREX_SPACEDIM == 3)
                -(phix(i,j,k+1)+phix(i,j,k-1))*term4
#endif
                ;
        }
        }
        }
    }

    if (do_y) {

        for (int k = ylo.z; k <= yhi.z; ++k) {
        for (int j = ylo.y; j <= yhi.y; ++j) {
        ioff = 0;
        if (offset == 2 && (ylo.x+j+k)%2 != (color+1)%2 ) {
	  ioff = 1;
	}
        AMREX_PRAGMA_SIMD
        for (int i = ylo.x+ioff; i <= yhi.x; i+=offset) {

            Lphiy(i,j,k) = phiy(i,j,k)*(theta_alpha*alphay(i,j,k) + term1)
                -(phiy(i+1,j,k)+phiy(i-1,j,k))*term2
                -(phiy(i,j+1,k)+phiy(i,j-1,k))*term3
#if (AMREX_SPACEDIM == 3)
                -(phiy(i,j,k+1)+phiy(i,j,k-1))*term4
#endif
                ;
        }
        }
        }
    }

#if (AMREX_SPACEDIM == 3)
    if (do_z) {

        for (int k = zlo.z; k <= zhi.z; ++k) {
        for (int j = zlo.y; j <= zhi.y; ++j) {
        ioff = 0;
        if (offset == 2 && (zlo.x+j+k)%2 != (color+1)%2 ) {
	  ioff = 1;
	}
        AMREX_PRAGMA_SIMD
        for (int i = zlo.x+ioff; i <= zhi.x; i+=offset) {

            Lphiz(i,j,k) = phiz(i,j,k)*(theta_alpha*alphaz(i,j,k) + term1)
                -(phiz(i+1,j,k)+phiz(i-1,j,k))*term2
                -(phiz(i,j+1,k)+phiz(i,j-1,k))*term3
                -(phiz(i,j,k+1)+phiz(i,j,k-1))*term4;
        }
        }
        }
    }
#endif

}

AMREX_GPU_HOST_DEVICE
inline
void stag_applyop_visc_m1 (Box const& tbx,
			   AMREX_D_DECL(Box const& xbx,
					Box const& ybx,
					Box const& zbx),
                           AMREX_D_DECL(Array4<Real const> const& alphax,
					Array4<Real const> const& alphay,
					Array4<Real const> const& alphaz),
                           AMREX_D_DECL(Array4<Real const> const& phix,
					Array4<Real const> const& phiy,
					Array4<Real const> const& phiz),
                           AMREX_D_DECL(Array4<Real> const& Lphix,
					Array4<Real> const& Lphiy,
					Array4<Real> const& Lphiz),
                           Array4<Real const> const& betacc,
                           Array4<Real const> const& betaxy,
#if (AMREX_SPACEDIM == 3)
                           Array4<Real const> const& betaxz,
                           Array4<Real const> const& betayz,
#endif
			   AMREX_D_DECL(bool do_x,
					bool do_y,
					bool do_z),
			   Real theta_alpha, Real bt,  Real gt, int offset,  int color,
			   const GpuArray<Real, AMREX_SPACEDIM> & dx) noexcept
{
    // xbx, ybx, and zbx are the face-centered boxes

    // if running on the host
    // tlo is the minimal box containins the union of the face-centered grid boxes

    // if running on the gpu, tlo is a box with a single point that comes
    // from the union of the face-centered grid boxes

    const auto tlo = lbound(tbx);
    const auto thi = ubound(tbx);

    // if running on the host, x/y/zlo and x/y/zhi are set to
    // the lower/uppser bounds of x/y/zbx

    // if running on the gpu, x/y/zlo and x/y/zhi are set to
    // the single point defined by tlo, unless tlo is outside of the union
    // of the face-centered grid boxes, in which case they are set to
    // values that make sure the loop is not entered

    AMREX_D_TERM(const auto xlo = amrex::elemwiseMax(tlo, lbound(xbx));,
                 const auto ylo = amrex::elemwiseMax(tlo, lbound(ybx));,
                 const auto zlo = amrex::elemwiseMax(tlo, lbound(zbx)););

    AMREX_D_TERM(const auto xhi = amrex::elemwiseMin(thi, ubound(xbx));,
                 const auto yhi = amrex::elemwiseMin(thi, ubound(ybx));,
                 const auto zhi = amrex::elemwiseMin(thi, ubound(zbx)););

    int ioff;

    Real dxsqinv = 1./(dx[0]*dx[0]);
    Real dysqinv = 1./(dx[1]*dx[1]);
#if (AMREX_SPACEDIM == 3)
    Real dzsqinv = 1./(dx[2]*dx[2]);
#endif

    if (do_x) {

        for (int k = xlo.z; k <= xhi.z; ++k) {
        for (int j = xlo.y; j <= xhi.y; ++j) {
        ioff = 0;
	if (offset == 2 && (xlo.x+j+k)%2 != (color+1)%2 ) {
	  ioff = 1;
	}
        AMREX_PRAGMA_SIMD
        for (int i = xlo.x+ioff; i <= xhi.x; i+=offset) {
            Lphix(i,j,k) = phix(i,j,k)*
                (theta_alpha*alphax(i,j,k)
                 +(betacc(i,j,k)+betacc(i-1,j,k))*dxsqinv
                 +(betaxy(i,j,k)+betaxy(i,j+1,k))*dysqinv
#if (AMREX_SPACEDIM == 3)                                        
                 +(betaxz(i,j,k)+betaxz(i,j,k+1))*dzsqinv
#endif
                )
                +(-phix(i+1,j,k)*betacc(i,j,k)-phix(i-1,j,k)*betacc(i-1,j,k))*dxsqinv
                +(-phix(i,j+1,k)*betaxy(i,j+1,k)-phix(i,j-1,k)*betaxy(i,j,k))*dysqinv
#if (AMREX_SPACEDIM == 3)                                        
                +(-phix(i,j,k+1)*betaxz(i,j,k+1)-phix(i,j,k-1)*betaxz(i,j,k))*dysqinv
#endif
                ;
        }
        }
        }
    }

    if (do_y) {

        for (int k = ylo.z; k <= yhi.z; ++k) {
        for (int j = ylo.y; j <= yhi.y; ++j) {
        ioff = 0;
        if (offset == 2 && (ylo.x+j+k)%2 != (color+1)%2 ) {
	  ioff = 1;
	}
        AMREX_PRAGMA_SIMD
        for (int i = ylo.x+ioff; i <= yhi.x; i+=offset) {
            Lphiy(i,j,k) = phiy(i,j,k)*
                (theta_alpha*alphay(i,j,k)
                 +(betacc(i,j,k)+betacc(i,j-1,k))*dysqinv
                 +(betaxy(i,j,k)+betaxy(i+1,j,k))*dxsqinv
#if (AMREX_SPACEDIM == 3)                                        
                 +(betayz(i,j,k)+betayz(i,j,k+1))*dzsqinv
#endif
                )
                +(-phiy(i,j+1,k)*betacc(i,j,k)-phiy(i,j-1,k)*betacc(i,j-1,k))*dysqinv
                +(-phiy(i+1,j,k)*betaxy(i+1,j,k)-phiy(i-1,j,k)*betaxy(i,j,k))*dxsqinv
#if (AMREX_SPACEDIM == 3)                                        
                +(-phiy(i,j,k+1)*betayz(i,j,k+1)-phiy(i,j,k-1)*betayz(i,j,k))*dzsqinv
#endif
                ;
        }
        }
        }
    }

#if (AMREX_SPACEDIM == 3)
    if (do_z) {

        for (int k = zlo.z; k <= zhi.z; ++k) {
        for (int j = zlo.y; j <= zhi.y; ++j) {
        ioff = 0;
        if (offset == 2 && (zlo.x+j+k)%2 != (color+1)%2 ) {
	  ioff = 1;
	}
        AMREX_PRAGMA_SIMD
        for (int i = zlo.x+ioff; i <= zhi.x; i+=offset) {
            Lphiz(i,j,k) = phiz(i,j,k)*
                (theta_alpha*alphaz(i,j,k)
                 +(betacc(i,j,k)+betacc(i,j,k-1))*dzsqinv
                 +(betaxz(i,j,k)+betaxz(i+1,j,k))*dxsqinv
                 +(betayz(i,j,k)+betayz(i,j+1,k))*dysqinv)
                +(-phiz(i,j,k+1)*betacc(i,j,k)-phiz(i,j,k-1)*betacc(i,j,k-1))*dzsqinv
                +(-phiz(i+1,j,k)*betaxz(i+1,j,k)-phiz(i-1,j,k)*betaxz(i,j,k))*dxsqinv
                +(-phiz(i,j+1,k)*betayz(i,j+1,k)-phiz(i,j-1,k)*betayz(i,j,k))*dysqinv;
        }
        }
        }
    }
#endif

}AMREX_GPU_HOST_DEVICE
inline
void stag_applyop_visc_p2 (Box const& tbx,
			   AMREX_D_DECL(Box const& xbx,
					Box const& ybx,
					Box const& zbx),
                           AMREX_D_DECL(Array4<Real const> const& alphax,
					Array4<Real const> const& alphay,
					Array4<Real const> const& alphaz),
                           AMREX_D_DECL(Array4<Real const> const& phix,
					Array4<Real const> const& phiy,
					Array4<Real const> const& phiz),
                           AMREX_D_DECL(Array4<Real> const& Lphix,
					Array4<Real> const& Lphiy,
					Array4<Real> const& Lphiz),
			   AMREX_D_DECL(bool do_x,
					bool do_y,
					bool do_z),
			   Real theta_alpha, Real bt,  Real gt, int offset,  int color,
			   const GpuArray<Real, AMREX_SPACEDIM> & dx) noexcept
{
    // xbx, ybx, and zbx are the face-centered boxes

    // if running on the host
    // tlo is the minimal box containins the union of the face-centered grid boxes

    // if running on the gpu, tlo is a box with a single point that comes
    // from the union of the face-centered grid boxes

    const auto tlo = lbound(tbx);
    const auto thi = ubound(tbx);

    // if running on the host, x/y/zlo and x/y/zhi are set to
    // the lower/uppser bounds of x/y/zbx

    // if running on the gpu, x/y/zlo and x/y/zhi are set to
    // the single point defined by tlo, unless tlo is outside of the union
    // of the face-centered grid boxes, in which case they are set to
    // values that make sure the loop is not entered

    AMREX_D_TERM(const auto xlo = amrex::elemwiseMax(tlo, lbound(xbx));,
                 const auto ylo = amrex::elemwiseMax(tlo, lbound(ybx));,
                 const auto zlo = amrex::elemwiseMax(tlo, lbound(zbx)););

    AMREX_D_TERM(const auto xhi = amrex::elemwiseMin(thi, ubound(xbx));,
                 const auto yhi = amrex::elemwiseMin(thi, ubound(ybx));,
                 const auto zhi = amrex::elemwiseMin(thi, ubound(zbx)););

    int ioff;

    Real dxsqinv = 1./(dx[0]*dx[0]);
    Real dysqinv = 1./(dx[1]*dx[1]);
    Real dxdyinv = 1./(dx[0]*dx[1]);
#if (AMREX_SPACEDIM == 3)
    Real dzsqinv = 1./(dx[2]*dx[2]);
    Real dxdzinv = 1./(dx[0]*dx[2]);
    Real dydzinv = 1./(dx[1]*dx[2]);
#endif

    if (do_x) {

#if (AMREX_SPACEDIM == 2)
        Real term1 = 2.*bt*(2.*dxsqinv+dysqinv);
#elif (AMREX_SPACEDIM == 3)
        Real term1 = 2.*bt*(2.*dxsqinv+dysqinv+dzsqinv);
#endif
    
        for (int k = xlo.z; k <= xhi.z; ++k) {
        for (int j = xlo.y; j <= xhi.y; ++j) {
        ioff = 0;
	if (offset == 2 && (xlo.x+j+k)%2 != (color+1)%2 ) {
	  ioff = 1;
	}
        AMREX_PRAGMA_SIMD
        for (int i = xlo.x+ioff; i <= xhi.x; i+=offset) {
            Lphix(i,j,k) = phix(i,j,k)*(theta_alpha*alphax(i,j,k) + term1)
                -bt*( (phix(i+1,j,k)+phix(i-1,j,k))*2.*dxsqinv
                      +(phix(i,j+1,k)+phix(i,j-1,k))*dysqinv
                      +(phiy(i,j+1,k)-phiy(i,j,k)-phiy(i-1,j+1,k)+phiy(i-1,j,k))*dxdyinv
#if (AMREX_SPACEDIM == 3)
                      +(phix(i,j,k+1)+phix(i,j,k-1))*dzsqinv
                      +(phiz(i,j,k+1)-phiz(i,j,k)-phiz(i-1,j,k+1)+phiz(i-1,j,k))*dxdzinv
#endif
                    );
        }
        }
        }
    }

    if (do_y) {

#if (AMREX_SPACEDIM == 2)
        Real term1 = 2.*bt*(dxsqinv+2.*dysqinv);
#elif (AMREX_SPACEDIM == 3)
        Real term1 = 2.*bt*(dxsqinv+2.*dysqinv+dzsqinv);
#endif

        for (int k = ylo.z; k <= yhi.z; ++k) {
        for (int j = ylo.y; j <= yhi.y; ++j) {
        ioff = 0;
        if (offset == 2 && (ylo.x+j+k)%2 != (color+1)%2 ) {
	  ioff = 1;
	}
        AMREX_PRAGMA_SIMD
        for (int i = ylo.x+ioff; i <= yhi.x; i+=offset) {
            Lphiy(i,j,k) = phiy(i,j,k)*( theta_alpha*alphay(i,j,k) + term1)
                -bt*( (phiy(i,j+1,k)+phiy(i,j-1,k))*2.*dysqinv
                      +(phiy(i+1,j,k)+phiy(i-1,j,k))*dxsqinv
                      +(phix(i+1,j,k)-phix(i,j,k)-phix(i+1,j-1,k)+phix(i,j-1,k))*dxdyinv
#if (AMREX_SPACEDIM == 3)
                      +(phiy(i,j,k+1)+phiy(i,j,k-1))*dzsqinv
                      +(phiz(i,j,k+1)-phiz(i,j,k)-phiz(i,j-1,k+1)+phiz(i,j-1,k))*dydzinv
#endif
                    );
        }
        }
        }
    }

#if (AMREX_SPACEDIM == 3)
    if (do_z) {

        Real term1 = 2.*bt*(dxsqinv+dysqinv+2.*dzsqinv);
        
        for (int k = zlo.z; k <= zhi.z; ++k) {
        for (int j = zlo.y; j <= zhi.y; ++j) {
        ioff = 0;
        if (offset == 2 && (zlo.x+j+k)%2 != (color+1)%2 ) {
	  ioff = 1;
	}
        AMREX_PRAGMA_SIMD
        for (int i = zlo.x+ioff; i <= zhi.x; i+=offset) {
            Lphiz(i,j,k) = phiz(i,j,k)*( theta_alpha*alphaz(i,j,k) + term1)
                -bt*( (phiz(i,j,k+1)+phiz(i,j,k-1))*2.*dzsqinv
                      +(phiz(i+1,j,k)+phiz(i-1,j,k))*dxsqinv
                      +(phiz(i,j+1,k)+phiz(i,j-1,k))*dysqinv
                      +(phix(i+1,j,k)-phix(i,j,k)-phix(i+1,j,k-1)+phix(i,j,k-1))*dxdzinv
                      +(phiy(i,j+1,k)-phiy(i,j,k)-phiy(i,j+1,k-1)+phiy(i,j,k-1))*dydzinv);
        }
        }
        }
    }
#endif

}

AMREX_GPU_HOST_DEVICE
inline
void stag_applyop_visc_m2 (Box const& tbx,
			   AMREX_D_DECL(Box const& xbx,
					Box const& ybx,
					Box const& zbx),
                           AMREX_D_DECL(Array4<Real const> const& alphax,
					Array4<Real const> const& alphay,
					Array4<Real const> const& alphaz),
                           AMREX_D_DECL(Array4<Real const> const& phix,
					Array4<Real const> const& phiy,
					Array4<Real const> const& phiz),
                           AMREX_D_DECL(Array4<Real> const& Lphix,
					Array4<Real> const& Lphiy,
					Array4<Real> const& Lphiz),
                           Array4<Real const> const& betacc,
                           Array4<Real const> const& betaxy,
#if (AMREX_SPACEDIM == 3)
                           Array4<Real const> const& betaxz,
                           Array4<Real const> const& betayz,
#endif
			   AMREX_D_DECL(bool do_x,
					bool do_y,
					bool do_z),
			   Real theta_alpha, Real bt,  Real gt, int offset,  int color,
			   const GpuArray<Real, AMREX_SPACEDIM> & dx) noexcept
{
    // xbx, ybx, and zbx are the face-centered boxes

    // if running on the host
    // tlo is the minimal box containins the union of the face-centered grid boxes

    // if running on the gpu, tlo is a box with a single point that comes
    // from the union of the face-centered grid boxes

    const auto tlo = lbound(tbx);
    const auto thi = ubound(tbx);

    // if running on the host, x/y/zlo and x/y/zhi are set to
    // the lower/uppser bounds of x/y/zbx

    // if running on the gpu, x/y/zlo and x/y/zhi are set to
    // the single point defined by tlo, unless tlo is outside of the union
    // of the face-centered grid boxes, in which case they are set to
    // values that make sure the loop is not entered

    AMREX_D_TERM(const auto xlo = amrex::elemwiseMax(tlo, lbound(xbx));,
                 const auto ylo = amrex::elemwiseMax(tlo, lbound(ybx));,
                 const auto zlo = amrex::elemwiseMax(tlo, lbound(zbx)););

    AMREX_D_TERM(const auto xhi = amrex::elemwiseMin(thi, ubound(xbx));,
                 const auto yhi = amrex::elemwiseMin(thi, ubound(ybx));,
                 const auto zhi = amrex::elemwiseMin(thi, ubound(zbx)););

    int ioff;

    Real dxsqinv = 1./(dx[0]*dx[0]);
    Real dysqinv = 1./(dx[1]*dx[1]);
    Real dxdyinv = 1./(dx[0]*dx[1]);
#if (AMREX_SPACEDIM == 3)
    Real dzsqinv = 1./(dx[2]*dx[2]);
    Real dxdzinv = 1./(dx[0]*dx[2]);
    Real dydzinv = 1./(dx[1]*dx[2]);
#endif

    if (do_x) {

        for (int k = xlo.z; k <= xhi.z; ++k) {
        for (int j = xlo.y; j <= xhi.y; ++j) {
        ioff = 0;
	if (offset == 2 && (xlo.x+j+k)%2 != (color+1)%2 ) {
	  ioff = 1;
	}
        AMREX_PRAGMA_SIMD
        for (int i = xlo.x+ioff; i <= xhi.x; i+=offset) {
                   
            Lphix(i,j,k) = phix(i,j,k)*
                ( theta_alpha*alphax(i,j,k) +
                  2.*(betacc(i,j,k)+betacc(i-1,j,k))*dxsqinv
                  + (betaxy(i,j,k)+betaxy(i,j+1,k))*dysqinv
#if (AMREX_SPACEDIM == 3)
                  + (betaxz(i,j,k)+betaxz(i,j,k+1))*dzsqinv
#endif
                    )
                        
                -2.*phix(i+1,j,k)*betacc(i,j,k)*dxsqinv
                -2.*phix(i-1,j,k)*betacc(i-1,j,k)*dxsqinv
                -phix(i,j+1,k)*betaxy(i,j+1,k)*dysqinv
                -phix(i,j-1,k)*betaxy(i,j,k)*dysqinv
#if (AMREX_SPACEDIM == 3)
                -phix(i,j,k+1)*betaxz(i,j,k+1)*dzsqinv
                -phix(i,j,k-1)*betaxz(i,j,k)*dzsqinv
#endif
                        
                -phiy(i,j+1,k)*betaxy(i,j+1,k)*dxdyinv
                +phiy(i,j,k)*betaxy(i,j,k)*dxdyinv
                +phiy(i-1,j+1,k)*betaxy(i,j+1,k)*dxdyinv
                -phiy(i-1,j,k)*betaxy(i,j,k)*dxdyinv
                        
#if (AMREX_SPACEDIM == 3)
                -phiz(i,j,k+1)*betaxz(i,j,k+1)*dxdzinv
                +phiz(i,j,k)*betaxz(i,j,k)*dxdzinv
                +phiz(i-1,j,k+1)*betaxz(i,j,k+1)*dxdzinv
                -phiz(i-1,j,k)*betaxz(i,j,k)*dxdzinv
#endif
                ;            
        }
        }
        }
    }

    if (do_y) {

        for (int k = ylo.z; k <= yhi.z; ++k) {
        for (int j = ylo.y; j <= yhi.y; ++j) {
        ioff = 0;
        if (offset == 2 && (ylo.x+j+k)%2 != (color+1)%2 ) {
	  ioff = 1;
	}
        AMREX_PRAGMA_SIMD
        for (int i = ylo.x+ioff; i <= yhi.x; i+=offset) {
                   
            Lphiy(i,j,k) = phiy(i,j,k)*
                ( theta_alpha*alphay(i,j,k) +
                  2.*(betacc(i,j,k)+betacc(i,j-1,k))*dysqinv
                  + (betaxy(i,j,k)+betaxy(i+1,j,k))*dxsqinv
#if (AMREX_SPACEDIM == 3)
                  + (betayz(i,j,k)+betayz(i,j,k+1))*dzsqinv
#endif
                    )
                        
                -2.*phiy(i,j+1,k)*betacc(i,j,k)*dysqinv
                -2.*phiy(i,j-1,k)*betacc(i,j-1,k)*dysqinv
                -phiy(i+1,j,k)*betaxy(i+1,j,k)*dxsqinv
                -phiy(i-1,j,k)*betaxy(i,j,k)*dxsqinv
#if (AMREX_SPACEDIM == 3)
                -phiy(i,j,k+1)*betayz(i,j,k+1)*dzsqinv
                -phiy(i,j,k-1)*betayz(i,j,k)*dzsqinv
#endif
                        
                -phix(i+1,j,k)*betaxy(i+1,j,k)*dxdyinv
                +phix(i,j,k)*betaxy(i,j,k)*dxdyinv
                +phix(i+1,j-1,k)*betaxy(i+1,j,k)*dxdyinv
                -phix(i,j-1,k)*betaxy(i,j,k)*dxdyinv
                        
#if (AMREX_SPACEDIM == 3)
                -phiz(i,j,k+1)*betayz(i,j,k+1)*dydzinv
                +phiz(i,j,k)*betayz(i,j,k)*dydzinv
                +phiz(i,j-1,k+1)*betayz(i,j,k+1)*dydzinv
                -phiz(i,j-1,k)*betayz(i,j,k)*dydzinv
#endif
                ;
        }
        }
        }
    }

#if (AMREX_SPACEDIM == 3)
    if (do_z) {

        for (int k = zlo.z; k <= zhi.z; ++k) {
        for (int j = zlo.y; j <= zhi.y; ++j) {
        ioff = 0;
        if (offset == 2 && (zlo.x+j+k)%2 != (color+1)%2 ) {
	  ioff = 1;
	}
        AMREX_PRAGMA_SIMD
        for (int i = zlo.x+ioff; i <= zhi.x; i+=offset) {

            Lphiz(i,j,k) = phiz(i,j,k)*
                ( theta_alpha*alphaz(i,j,k) +
                  2.*(betacc(i,j,k)+betacc(i,j,k-1))*dzsqinv
                  + (betaxz(i,j,k)+betaxz(i+1,j,k))*dxsqinv
                  + (betayz(i,j,k)+betayz(i,j+1,k))*dysqinv )
                        
                -2.*phiz(i,j,k+1)*betacc(i,j,k)*dzsqinv
                -2.*phiz(i,j,k-1)*betacc(i,j,k-1)*dzsqinv
                -phiz(i+1,j,k)*betaxz(i+1,j,k)*dxsqinv
                -phiz(i-1,j,k)*betaxz(i,j,k)*dxsqinv
                -phiz(i,j+1,k)*betayz(i,j+1,k)*dysqinv
                -phiz(i,j-1,k)*betayz(i,j,k)*dysqinv
                        
                -phix(i+1,j,k)*betaxz(i+1,j,k)*dxdzinv
                +phix(i,j,k)*betaxz(i,j,k)*dxdzinv
                +phix(i+1,j,k-1)*betaxz(i+1,j,k)*dxdzinv
                -phix(i,j,k-1)*betaxz(i,j,k)*dxdzinv
                        
                -phiy(i,j+1,k)*betayz(i,j+1,k)*dydzinv
                +phiy(i,j,k)*betayz(i,j,k)*dydzinv
                +phiy(i,j+1,k-1)*betayz(i,j+1,k)*dydzinv
                -phiy(i,j,k-1)*betayz(i,j,k)*dydzinv;            
        }
        }
        }
    }
#endif
}

void StagApplyOp(const Geometry & geom,
                 const MultiFab& beta_cc,
                 const MultiFab& gamma_cc,
                 const std::array<MultiFab, NUM_EDGE>& beta_ed,
                 const std::array<MultiFab, AMREX_SPACEDIM>& phi,
                 std::array<MultiFab, AMREX_SPACEDIM>& Lphi,
                 const std::array<MultiFab, AMREX_SPACEDIM>& alpha_fc,
                 const Real* dx,
                 const amrex::Real& theta_alpha,
                 const int& color)
{

    BL_PROFILE_VAR("StagApplyOp()",StagApplyOp);
    
    GpuArray<Real,AMREX_SPACEDIM> dx_gpu{AMREX_D_DECL(dx[0], dx[1], dx[2])};
    
    AMREX_D_DECL(bool do_x,do_y,do_z);

    int offset = 1;

    if (color == 0) {
        AMREX_D_TERM(do_x = true;,
                     do_y = true,;
                     do_z = true;);

    }
    else if (color == 1 || color == 2) {
        AMREX_D_TERM(do_x = true;,
                     do_y = false;,
                     do_z = false;);
        offset = 2;
    }
    else if (color == 3 || color == 4) {
        AMREX_D_TERM(do_x = false;,
                     do_y = true;,
                     do_z = false;);
        offset = 2;
    }
#if (AMREX_SPACEDIM == 3)
    else if (color == 5 || color == 6) {
        AMREX_D_TERM(do_x = false;,
                     do_y = false;,
                     do_z = true;);
        offset = 2;
    }
#endif
    else {
        Abort("StagApplyOp: Invalid Color");
    }

    // Loop over boxes (make sure mfi takes a cell-centered multifab as an argument)
    for (MFIter mfi(beta_cc,TilingIfNotGPU()); mfi.isValid(); ++mfi) {

        const Box & bx = mfi.tilebox();

        Array4<Real const> const& beta_cc_fab = beta_cc.array(mfi);
        Array4<Real const> const& gamma_cc_fab = gamma_cc.array(mfi);

        Array4<Real const> const& beta_xy_fab = beta_ed[0].array(mfi);
#if (AMREX_SPACEDIM == 3)
        Array4<Real const> const& beta_xz_fab = beta_ed[1].array(mfi);
        Array4<Real const> const& beta_yz_fab = beta_ed[2].array(mfi);
#endif

        AMREX_D_TERM(Array4<Real const> const& phix_fab = phi[0].array(mfi);,
                     Array4<Real const> const& phiy_fab = phi[1].array(mfi);,
                     Array4<Real const> const& phiz_fab = phi[2].array(mfi););

        AMREX_D_TERM(Array4<Real> const& Lphix_fab = Lphi[0].array(mfi);,
                     Array4<Real> const& Lphiy_fab = Lphi[1].array(mfi);,
                     Array4<Real> const& Lphiz_fab = Lphi[2].array(mfi););

        AMREX_D_TERM(Array4<Real const> const& alphax_fab = alpha_fc[0].array(mfi);,
                     Array4<Real const> const& alphay_fab = alpha_fc[1].array(mfi);,
                     Array4<Real const> const& alphaz_fab = alpha_fc[2].array(mfi););

        AMREX_D_TERM(const Box& bx_x = mfi.nodaltilebox(0);,
                     const Box& bx_y = mfi.nodaltilebox(1);,
                     const Box& bx_z = mfi.nodaltilebox(2););

        const Box& index_bounds = amrex::getIndexBounds(AMREX_D_DECL(bx_x,bx_y,bx_z));

        Real bt, gt;
        // for positive visc_types, the coefficients are constant in space
        if (visc_type > 0) {
            const auto& lo = amrex::lbound(bx);
            bt = beta_cc_fab (lo.x,lo.y,lo.z);
            gt = gamma_cc_fab(lo.x,lo.y,lo.z);
        }

        if (visc_type == 1) {
            AMREX_LAUNCH_HOST_DEVICE_LAMBDA(index_bounds, tbx,
            {
                stag_applyop_visc_p1(tbx, AMREX_D_DECL(bx_x,bx_y,bx_z),
                                     AMREX_D_DECL(alphax_fab,alphay_fab,alphaz_fab),
                                     AMREX_D_DECL(phix_fab,phiy_fab,phiz_fab),
                                     AMREX_D_DECL(Lphix_fab,Lphiy_fab,Lphiz_fab),
                                     AMREX_D_DECL(do_x,do_y,do_z),
                                     theta_alpha, bt, gt, offset, color, dx_gpu);
            });
        }
        else if (visc_type == -1) {
            AMREX_LAUNCH_HOST_DEVICE_LAMBDA(index_bounds, tbx,
            {
                stag_applyop_visc_m1(tbx, AMREX_D_DECL(bx_x,bx_y,bx_z),
                                     AMREX_D_DECL(alphax_fab,alphay_fab,alphaz_fab),
                                     AMREX_D_DECL(phix_fab,phiy_fab,phiz_fab),
                                     AMREX_D_DECL(Lphix_fab,Lphiy_fab,Lphiz_fab),
                                     beta_cc_fab, beta_xy_fab,
#if (AMREX_SPACEDIM == 3)
                                     beta_xz_fab, beta_yz_fab,
#endif                                 
                                     AMREX_D_DECL(do_x,do_y,do_z),
                                     theta_alpha, bt, gt, offset, color, dx_gpu);
            });
        }
        else if (visc_type == 2) {
            AMREX_LAUNCH_HOST_DEVICE_LAMBDA(index_bounds, tbx,
            {
                stag_applyop_visc_p2(tbx, AMREX_D_DECL(bx_x,bx_y,bx_z),
                                     AMREX_D_DECL(alphax_fab,alphay_fab,alphaz_fab),
                                     AMREX_D_DECL(phix_fab,phiy_fab,phiz_fab),
                                     AMREX_D_DECL(Lphix_fab,Lphiy_fab,Lphiz_fab),
                                     AMREX_D_DECL(do_x,do_y,do_z),
                                     theta_alpha, bt, gt, offset, color, dx_gpu);
            });
        }
        else if (visc_type == -2) {
            AMREX_LAUNCH_HOST_DEVICE_LAMBDA(index_bounds, tbx,
            {
                stag_applyop_visc_m2(tbx, AMREX_D_DECL(bx_x,bx_y,bx_z),
                                     AMREX_D_DECL(alphax_fab,alphay_fab,alphaz_fab),
                                     AMREX_D_DECL(phix_fab,phiy_fab,phiz_fab),
                                     AMREX_D_DECL(Lphix_fab,Lphiy_fab,Lphiz_fab),
                                     beta_cc_fab, beta_xy_fab,
#if (AMREX_SPACEDIM == 3)
                                     beta_xz_fab, beta_yz_fab,
#endif                                 
                                     AMREX_D_DECL(do_x,do_y,do_z),
                                     theta_alpha, bt, gt, offset, color, dx_gpu);
            });            
        }
    }

    for (int i=0; i<AMREX_SPACEDIM; ++i) {
        MultiFABPhysBCDomainVel(Lphi[i], geom, i);
    }
    
}
