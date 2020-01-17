#include <incflo.H>
#include <prob_bc.H>

using namespace amrex;

void incflo::fillphysbc_velocity (int lev, Real time, MultiFab& vel, int ng)
{
    PhysBCFunct<GpuBndryFuncFab<IncfloVelFill> > physbc(geom[lev], get_velocity_bcrec(),
                                                        IncfloVelFill{probtype, m_bc_velocity});
    physbc.FillBoundary(vel, 0, AMREX_SPACEDIM, IntVect(ng), time, 0);
}

void incflo::fillphysbc_density (int lev, Real time, MultiFab& density, int ng)
{
    PhysBCFunct<GpuBndryFuncFab<IncfloDenFill> > physbc(geom[lev], get_density_bcrec(),
                                                        IncfloDenFill{probtype, m_bc_density});
    physbc.FillBoundary(density, 0, 1, IntVect(ng), time, 0);
}

void incflo::fillphysbc_tracer (int lev, Real time, MultiFab& tracer, int ng)
{
    if (ntrac > 0) {
        PhysBCFunct<GpuBndryFuncFab<IncfloTracFill> > physbc
            (geom[lev], get_tracer_bcrec(), IncfloTracFill{probtype, ntrac, m_bc_tracer_d});
        physbc.FillBoundary(tracer, 0, ntrac, IntVect(ng), time, 0);
    }
}