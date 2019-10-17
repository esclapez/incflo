#include <incflo.H>
#include <incflo_divop_conv.hpp>
#include <param_mod_F.H>

#include <AMReX_REAL.H>
#include <AMReX_BLFort.H>
#include <AMReX_SPACE.H>
#include <AMReX_Array.H>

//
// Compute the three components of the convection term
//
void
incflo::incflo_compute_convective_term( Vector< std::unique_ptr<MultiFab> >& conv_u_in, 
                                        Vector< std::unique_ptr<MultiFab> >& conv_s_in,
                                        Vector< std::unique_ptr<MultiFab> >& vel_in,
                                        Vector< std::unique_ptr<MultiFab> >& density_in,
                                        Vector< std::unique_ptr<MultiFab> >& tracer_in,
                                        Real time)
{
    BL_PROFILE("incflo::incflo_compute_convective_term");

    // Temporaries to store fluxes 
    Vector< std::unique_ptr<MultiFab> > fx;
    Vector< std::unique_ptr<MultiFab> > fy;
    Vector< std::unique_ptr<MultiFab> > fz;

    fx.resize(nlev);
    fy.resize(nlev);
    fz.resize(nlev);

    int icomp; int ncomp; int slopes_comp; int conv_comp; int state_comp; int num_comp;

    // First do FillPatch of {velocity, density, tracer} so we know the ghost cells of
    // these arrays are all filled
    for (int lev = 0; lev <= finest_level; lev++)
    {
        // State with ghost cells
        MultiFab Sborder_u(grids[lev], dmap[lev], vel_in[lev]->nComp(), nghost,
                           MFInfo(), *ebfactory[lev]);
        FillPatchVel(lev, time, Sborder_u, 0, Sborder_u.nComp());

        // Copy each FAB back from Sborder_u into the vel array, complete with filled ghost cells
        MultiFab::Copy (*vel_in[lev], Sborder_u, 0, 0, vel_in[lev]->nComp(), vel_in[lev]->nGrow());

        MultiFab Sborder_s(grids[lev], dmap[lev], 1, nghost, MFInfo(), *ebfactory[lev]);

        // Note that we FillPatch density even if not advecting it to be sure we have the 
        // right bc's for the variable density MAC projection
        icomp = 0; ncomp = 1;
        FillPatchScalar(lev, time, Sborder_s, icomp, ncomp);
        MultiFab::Copy (*density_in[lev], Sborder_s, 0, 0, 1, density_in[lev]->nGrow());

        if (advect_tracer)
        {
           icomp = 1; ncomp = 1;
           FillPatchScalar(lev, time, Sborder_s, icomp, ncomp);
           MultiFab::Copy (*tracer_in[lev], Sborder_s, 0, 0, 1, tracer_in[lev]->nGrow());
        }
 
        // We make these with ncomp = 3 so they can hold all three velocity components at once;
        //    note we can also use them to just hold the single density or tracer comp
        fx[lev].reset(new MultiFab(m_u_mac[lev]->boxArray(),dmap[lev],3,2,MFInfo(),*ebfactory[lev]));
        fy[lev].reset(new MultiFab(m_v_mac[lev]->boxArray(),dmap[lev],3,2,MFInfo(),*ebfactory[lev]));
        fz[lev].reset(new MultiFab(m_w_mac[lev]->boxArray(),dmap[lev],3,2,MFInfo(),*ebfactory[lev]));
 
        // We need this to avoid FPE
        m_u_mac[lev]->setVal(covered_val);
        m_v_mac[lev]->setVal(covered_val);
        m_w_mac[lev]->setVal(covered_val);

        fx[lev]->setVal(covered_val);
        fy[lev]->setVal(covered_val);
        fz[lev]->setVal(covered_val);
 
        // Predict normal velocity to faces -- note that the {u_mac, v_mac, w_mac}
        //    arrays returned from this call are on face CENTROIDS
        incflo_predict_vels_on_faces(lev, time, vel_in);
    }

    // Do projection on all AMR levels in one shot -- note that the {u_mac, v_mac, w_mac}
    //    arrays returned from this call are in on face CENTROIDS
    apply_MAC_projection (m_u_mac, m_v_mac, m_w_mac, density_in, time);

    bool already_on_centroids = true;

    Array<MultiFab*,AMREX_SPACEDIM> fluxes;

    for (int lev=0; lev < nlev; ++lev)
    {

        fluxes[0] = fx[lev].get();
        fluxes[1] = fy[lev].get();
        fluxes[2] = fz[lev].get();

        // We make this with ncomp = 3 so it can hold all three velocity components at once;
        //    note we can also use it to just hold the single density or tracer comp
        // We note that it needs two ghost cells for the redistribution step.
        MultiFab conv_tmp(grids[lev], dmap[lev], 3, 2, MFInfo(), *ebfactory[lev]);
        conv_tmp.setVal(0.);

        if (advect_tracer)
        {
            // Convert tracer to (rho * tracer) so we can use conservative update
            MultiFab::Multiply(*tracer_in[lev],*density_in[lev],0,0,1,tracer_in[lev]->nGrow());
        }

        // Compute slopes of density and tracer
        if (advect_density)
        {
           slopes_comp = 0;
           incflo_compute_slopes(lev, time, *density_in[lev], xslopes_s, yslopes_s, zslopes_s, slopes_comp);
        }

        if (advect_tracer)
        {
           slopes_comp = 1;
           incflo_compute_slopes(lev, time, *tracer_in[lev], xslopes_s, yslopes_s, zslopes_s, slopes_comp);
        }

        // Initialize conv_s to 0 for both density and tracer
        conv_s_in[lev]->setVal(0.,0,conv_s_in[lev]->nComp(),conv_s_in[lev]->nGrow());

        // **************************************************
        // Compute div (u u) -- the update for velocity
        // **************************************************
        conv_comp = 0; state_comp = 0; num_comp = 3; slopes_comp = 0;
        incflo_compute_fluxes(lev, fx, fy, fz, vel_in, state_comp, num_comp,
                            xslopes_u, yslopes_u, zslopes_u, slopes_comp,
                            m_u_mac, m_v_mac, m_w_mac);

        EB_computeDivergence(conv_tmp, GetArrOfConstPtrs(fluxes), geom[lev], already_on_centroids);
        incflo_redistribute(lev, conv_tmp, conv_u_in, conv_comp, num_comp);

        // **************************************************
        // Compute div (rho u) -- the update for density
        // **************************************************
        if (advect_density)
        {
            conv_comp = 0; state_comp = 0; num_comp = 1; slopes_comp = 0;
            incflo_compute_fluxes(lev, fx, fy, fz, density_in, state_comp, num_comp,
                                xslopes_s, yslopes_s, zslopes_s, slopes_comp,
                                m_u_mac, m_v_mac, m_w_mac);
            EB_computeDivergence(conv_tmp, GetArrOfConstPtrs(fluxes), geom[lev], already_on_centroids);
            incflo_redistribute(lev, conv_tmp, conv_s_in, conv_comp, num_comp);
        }

        // **********************************************************
        // Compute div (rho trac u) -- the update for (rho*trac)
        // **********************************************************
        if (advect_tracer)
        {
            conv_comp = 1; state_comp = 0; num_comp = 1; slopes_comp = 1;
            incflo_compute_fluxes(lev, fx, fy, fz, tracer_in, state_comp, num_comp,
                                xslopes_s, yslopes_s, zslopes_s, slopes_comp,
                                m_u_mac, m_v_mac, m_w_mac);
            EB_computeDivergence(conv_tmp, GetArrOfConstPtrs(fluxes), geom[lev], already_on_centroids);
            incflo_redistribute(lev, conv_tmp, conv_s_in, conv_comp, num_comp);
        }

        if (advect_tracer)
        {
           // Convert (rho * tracer) back to tracer
           MultiFab::Divide(*tracer_in[lev],*density_in[lev],0,0,1,tracer_in[lev]->nGrow());
        }

        // Return the negative
        conv_u_in[lev] -> mult(-1.0);
        conv_s_in[lev] -> mult(-1.0);

    } // lev
}

void
incflo::incflo_redistribute( int lev, 
                         MultiFab& conv_tmp_in, 
                         Vector< std::unique_ptr<MultiFab> >& conv_out,
                         int conv_comp, int ncomp) 
{
    Box domain(geom[lev].Domain());

    EB_set_covered(conv_tmp_in, covered_val);
    conv_tmp_in.FillBoundary(geom[lev].periodicity());

    const amrex::MultiFab*                    volfrac;

    volfrac   = &(ebfactory[lev] -> getVolFrac());

    for (MFIter mfi(*conv_out[lev],TilingIfNotGPU()); mfi.isValid(); ++mfi)
    {
          // Tilebox
          Box bx = mfi.tilebox ();

          // this is to check efficiently if this tile contains any eb stuff
          const EBFArrayBox&  conv_fab = static_cast<EBFArrayBox const&>((*conv_out[lev])[mfi]);
          const EBCellFlagFab&  flags = conv_fab.getEBCellFlagFab();

          if (flags.getType(amrex::grow(bx,0)) == FabType::covered )
          {
              // If tile is completely covered by EB geometry, 
              // set term to some very large number so we know if
              // we accidentally use these later in calculations
              (*conv_out[lev])[mfi].setVal( get_my_huge(), bx, conv_comp, ncomp);
          }
          else
          {
             // No cut cells in tile + nghost-cell witdh halo -> use non-eb routine
             if (flags.getType(amrex::grow(bx,nghost)) == FabType::regular )
             {
                (*conv_out[lev])[mfi].copy(conv_tmp_in[mfi],bx,0,bx,conv_comp,ncomp);
             }
             else
             {
                const int cyclic_x = geom[0].isPeriodic(0) ? 1 : 0;
                const int cyclic_y = geom[0].isPeriodic(1) ? 1 : 0;
                const int cyclic_z = geom[0].isPeriodic(2) ? 1 : 0;

                // Compute div(tau) with EB algorithm
                incflo_apply_eb_redistribution(bx, *conv_out[lev], conv_tmp_in, &mfi,
                                             conv_comp, ncomp, flags, volfrac, domain,
                                             cyclic_x, cyclic_y, cyclic_z,
                                             geom[lev].CellSize());

             }
          }
    }
}

