#ifdef AMREX_USE_EB

#include <Redistribution.H>
#include <AMReX_EB_slopes_K.H>

using namespace amrex;

#if (AMREX_SPACEDIM == 3)
void
redistribution::make_itracker (
                       Box const& bx, 
                       Array4<Real const> const& apx,
                       Array4<Real const> const& apy,
                       Array4<Real const> const& apz,
                       Array4<Real const> const& vfrac,
                       Array4<int> const& itracker,
                       Geometry& lev_geom)
{
    bool debug_print = false;

    const Box domain = lev_geom.Domain();

    // Note that itracker has 8 components and all are initialized to zero
    // We will add to the first component every time this cell is included in a merged neighborhood, 
    //    either by merging or being merged
    // We identify the cells in the remaining three components with the following ordering
    // 
    //    at k-1   |   at k  |   at k+1 
    // 
    // ^  15 16 17 |  6 7 8  |  24 25 26
    // |  12 13 14 |  4   5  |  21 22 23
    // j  9  10 11 |  1 2 3  |  18 19 20
    //   i --->
    // 
    // Note the first component of each of these arrays should never be used
    Array<int,27>    imap{0,-1, 0, 1,-1, 1,-1, 0, 1,-1, 0, 1,-1, 0, 1,-1, 0, 1,-1, 0, 1,-1, 0, 1,-1, 0, 1};
    Array<int,27>    jmap{0,-1,-1,-1, 0, 0, 1, 1, 1,-1,-1,-1, 0, 0, 0, 1, 1, 1,-1,-1,-1, 0, 0, 0, 1, 1, 1};
    Array<int,27>    kmap{0, 0, 0, 0, 0, 0, 0, 0, 0,-1,-1,-1,-1,-1,-1,-1,-1,-1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
    Array<int,27> inv_map{0, 8, 7, 6, 5, 4, 3, 2, 1,26,25,24,23,22,21,20,19,18,17,16,15,14,13,12,11,10, 9};

    const Real small_norm = 1.e-8;

    const auto& is_periodic_x = lev_geom.isPeriodic(0);
    const auto& is_periodic_y = lev_geom.isPeriodic(1);
    const auto& is_periodic_z = lev_geom.isPeriodic(2);

    if (debug_print)
        amrex::Print() << " IN MERGE_REDISTRIBUTE DOING BOX " << bx << std::endl;

    amrex::ParallelFor(bx, 
    [=] AMREX_GPU_DEVICE (int i, int j, int k) noexcept
    {
       if (vfrac(i,j,k) > 0.0 && vfrac(i,j,k) < 0.5)
       {
           Real apnorm, apnorm_inv;
           const Real dapx = apx(i+1,j  ,k  ) - apx(i,j,k);
           const Real dapy = apy(i  ,j+1,k  ) - apy(i,j,k);
           const Real dapz = apz(i  ,j  ,k+1) - apz(i,j,k);
           apnorm = std::sqrt(dapx*dapx+dapy*dapy+dapz*dapz);
           apnorm_inv = 1.0/apnorm;
           Real nx = dapx * apnorm_inv;
           Real ny = dapy * apnorm_inv;
           Real nz = dapz * apnorm_inv;

           // We use small_norm as an offset just to break the tie when at 45 degrees ...
           // Note that x-direction is preferred, followed by y-direction
           nz -= 2.*small_norm;
           ny -= small_norm;

           bool xdir_ok = is_periodic_x || (i != domain.smallEnd(0) && i != domain.bigEnd(0)) ;
           bool ydir_ok = is_periodic_y || (j != domain.smallEnd(1) && j != domain.bigEnd(1)) ;
           bool zdir_ok = is_periodic_z || (k != domain.smallEnd(2) && k != domain.bigEnd(2)) ;

           // x-component of normal is greatest
           if ( (std::abs(nx) > std::abs(ny)) &&
                (std::abs(nx) > std::abs(nz)) )
           {
               if (nx > 0) 
                   itracker(i,j,k,1) = 5;
               else
                   itracker(i,j,k,1) = 4;

           // y-component of normal is greatest
           } else if ( (std::abs(ny) >= std::abs(nx)) &&
                       (std::abs(ny) > std::abs(nz)) )
           {
               if (ny > 0) 
                   itracker(i,j,k,1) = 7;
               else

                   itracker(i,j,k,1) = 2;
           // z-component of normal is greatest
           } else {
               if (nz > 0) 
                   itracker(i,j,k,1) = 22;
               else
                   itracker(i,j,k,1) = 13;
           }
    
           // Override above logic if at a domain boundary (and non-periodic)
           if (!xdir_ok)
           {
               if ( (std::abs(ny) > std::abs(nz)) )
               {
                   if (ny > 0) 
                       itracker(i,j,k,1) = 7;
                   else 
                       itracker(i,j,k,1) = 2;
               } else {
                   if (nz > 0) 
                       itracker(i,j,k,1) = 22;
                   else 
                       itracker(i,j,k,1) = 13;
               }
           }
           if (!ydir_ok)
           {
               if ( (std::abs(nx) > std::abs(nz)) )
               {
                   if (nx > 0) 
                       itracker(i,j,k,1) = 5;
                   else 
                       itracker(i,j,k,1) = 4;
               } else {
                   if (nz > 0) 
                       itracker(i,j,k,1) = 22;
                   else 
                       itracker(i,j,k,1) = 13;
               }
           }
           if (!zdir_ok)
           {
               if ( (std::abs(nx) > std::abs(ny)) )
               {
                   if (nx > 0) 
                       itracker(i,j,k,1) = 5;
                   else 
                       itracker(i,j,k,1) = 4;
               } else {
                   if (ny > 0) 
                       itracker(i,j,k,1) = 7;
                   else 
                       itracker(i,j,k,1) = 2;
               }
           }

           // (i,j,k) merges with at least one cell now
           itracker(i,j,k,0) += 1;

           // (i+ioff,j+joff,k+koff) is now the first cell in the nbhd of (i,j,k)
           int ioff = imap[itracker(i,j,k,1)];
           int joff = jmap[itracker(i,j,k,1)];
           int koff = kmap[itracker(i,j,k,1)];

           // Sanity check
           if (vfrac(i+ioff,j+joff,k+koff) == 0.)
               amrex::Abort(" Trying to merge with covered cell");

           Real sum_vol = vfrac(i,j,k) + vfrac(i+ioff,j+joff,k+koff);

           if (debug_print)
               amrex::Print() << "Cell " << IntVect(i,j,k) << " with volfrac " << vfrac(i,j,k) << 
                                 " trying to merge with " << IntVect(i+ioff,j+joff,k+koff) <<
                                 " with volfrac " << vfrac(i+ioff,j+joff,k+koff) << 
                                 " to get new sum_vol " <<  sum_vol << std::endl;

           // If the merged cell isn't large enough, we can merge in one of the other directions
           if (sum_vol < 0.5)
           {
               // Original offset was in x-direction
               if (joff == 0 and koff == 0)
               {
                   if ( (std::abs(ny) > std::abs(nz)) )
                   {
                       if (ny > 0) 
                           itracker(i,j,k,2) = 7;
                       else 
                           itracker(i,j,k,2) = 2;
                   } else {
                       if (nz > 0) 
                           itracker(i,j,k,2) = 22;
                       else 
                           itracker(i,j,k,2) = 13;
                   }

               // Original offset was in y-direction
               } else if (ioff == 0 and koff == 0)
               {
                   if ( (std::abs(nx) > std::abs(nz)) )
                   {
                       if (nx > 0) 
                           itracker(i,j,k,2) = 5;
                       else 
                           itracker(i,j,k,2) = 4;
                   } else {
                       if (nz > 0) 
                           itracker(i,j,k,2) = 22;
                       else 
                           itracker(i,j,k,2) = 13;
                   }

               // Original offset was in z-direction
               } else if (ioff == 0 and joff == 0)
               {
                   if ( (std::abs(nx) > std::abs(ny)) )
                   {
                       if (nx > 0) 
                           itracker(i,j,k,2) = 5;
                       else 
                           itracker(i,j,k,2) = 4;
                   } else {
                       if (ny > 0) 
                           itracker(i,j,k,2) = 7;
                       else 
                           itracker(i,j,k,2) = 2;
                   }
               }

               // (i,j,k) merges with at least two cells now
               itracker(i,j,k,0) += 1;

               // (i+ioff,j+joff,k+koff) is in the nbhd of (i,j,k)
               int ioff = imap[itracker(i,j,k,2)];
               int joff = jmap[itracker(i,j,k,2)];
               int koff = kmap[itracker(i,j,k,2)];

               sum_vol += vfrac(i+ioff,j+joff,k+koff);
               if (debug_print)
                   amrex::Print() << "Cell " << IntVect(i,j,k) << " with volfrac " << vfrac(i,j,k) << 
                                     " trying to ALSO merge with " << IntVect(i+ioff,j+joff,k+koff) <<
                                     " with volfrac " << vfrac(i+ioff,j+joff,k+koff) << 
                                      " to get new sum_vol " <<  sum_vol << std::endl;
           }
        
           // If the merged cell has merged in two directions, we now merge in the corner direction within the current plane
           if (itracker(i,j,k,0) >= 2)
           {
               // We already have two offsets, and we know they are in different directions 
               ioff = imap[itracker(i,j,k,1)] + imap[itracker(i,j,k,2)];
               joff = jmap[itracker(i,j,k,1)] + jmap[itracker(i,j,k,2)];
               koff = kmap[itracker(i,j,k,1)] + kmap[itracker(i,j,k,2)];

               // Both nbors are in the koff=0 plane
               if (koff == 0)
               {
                   if (ioff > 0 and joff > 0)
                       itracker(i,j,k,3) = 8;
                   else if (ioff < 0 and joff > 0)
                       itracker(i,j,k,3) = 6;
                   else if (ioff > 0 and joff < 0)
                       itracker(i,j,k,3) = 3;
                   else 
                       itracker(i,j,k,3) = 1;

               // Both nbors are in the joff=0 plane
               } else if (joff == 0) {
                   if (ioff > 0 and koff > 0)
                       itracker(i,j,k,3) = 23;
                   else if (ioff < 0 and koff > 0)
                       itracker(i,j,k,3) = 21;
                   else if (ioff > 0 and koff < 0)
                       itracker(i,j,k,3) = 14;
                   else 
                       itracker(i,j,k,3) = 12;

               // Both nbors are in the ioff=0 plane
               } else {
                   if (joff > 0 and koff > 0)
                       itracker(i,j,k,3) = 25;
                   else if (joff < 0 and koff > 0)
                       itracker(i,j,k,3) = 19;
                   else if (joff > 0 and koff < 0)
                       itracker(i,j,k,3) = 16;
                   else 
                       itracker(i,j,k,3) = 10;
               }

               // (i,j,k) merges with at least three cells now
               itracker(i,j,k,0) += 1;

               sum_vol += vfrac(i+ioff,j+joff,k+koff);
               if (debug_print)
                    amrex::Print() << "Cell " << IntVect(i,j,k) << " with volfrac " << vfrac(i,j,k) << 
                                      " trying to ALSO merge with " << IntVect(i+ioff,j+joff,k+koff) <<
                                      " with volfrac " << vfrac(i+ioff,j+joff,k+koff) << 
                                      " to get new sum_vol " <<  sum_vol << std::endl;
           }
       }
    });

    // At this point every cell knows who it wants to merge with, but
    //   (1) not who wants to merge with it
    //   (2) not who its neighbor also wants to merge with
    // In this loop we only address (1)
    amrex::ParallelFor(bx, 
    [=] AMREX_GPU_DEVICE (int i, int j, int k) noexcept
    {
       // Here we don't test on vfrac because some of the neighbors are full cells
       // We test on whether this cell has any neighbors
       if (itracker(i,j,k,0) > 0)
       {
          for (int ipair = 1; ipair <= itracker(i,j,k,0); ipair++)
          {
               // (i+ioff,j+joff,k+koff) is in the nbhd of (i,j,k)
               int ioff = imap[itracker(i,j,k,ipair)];
               int joff = jmap[itracker(i,j,k,ipair)];
               int koff = kmap[itracker(i,j,k,ipair)];

               int n_of_nbor = itracker(i+ioff,j+joff,k+koff,0);
               bool found = false;
               for (int ipair_nbor = 1; ipair_nbor <= n_of_nbor; ipair_nbor++)
               {
                   if (imap[itracker(i+ioff,j+joff,k+koff,ipair_nbor)] + ioff == 0 && 
                       jmap[itracker(i+ioff,j+joff,k+koff,ipair_nbor)] + joff == 0 &&
                       kmap[itracker(i+ioff,j+joff,k+koff,ipair_nbor)] + koff == 0)
                       found = true;
               }
               if  (!found)  
               {
                   // My neigbor didn't know about me so add me to my nbor's list of neighbors
                   itracker(i+ioff,j+joff,k+koff,0) += 1;
                   itracker(i+ioff,j+joff,k+koff,n_of_nbor+1) = inv_map[itracker(i,j,k,ipair)];
                   if (debug_print)
                       amrex::Print() << "Cell   " << IntVect(i,j,k) << " had nbor " << IntVect(i+ioff,j+joff,k+koff) 
                                      << " in its nbor list by taking inverse of " << itracker(i,j,k,ipair) 
                                      << " which gave " << inv_map[itracker(i,j,k,ipair)] << std::endl;
                   if (debug_print)
                        amrex::Print() << "Adding " << IntVect(i+ioff+imap[itracker(i+ioff,j+joff,k+koff,n_of_nbor+1)],
                                                               j+joff+jmap[itracker(i+ioff,j+joff,k+koff,n_of_nbor+1)],
                                                               k+koff+kmap[itracker(i+ioff,j+joff,k+koff,n_of_nbor+1)])
                                       << " to the nbor list of " << IntVect(i+ioff,j+joff,k+koff) << std::endl;
               }
          }
       }
    });

    // Here we address (2), i.e. we want the neighbor of my neighbor to be my neighbor
    amrex::ParallelFor(bx, 
    [=] AMREX_GPU_DEVICE (int i, int j, int k) noexcept
    {
       // Test on whether this cell has any neighbors and not already all seven neighbors
       if (itracker(i,j,k,0) > 0 && itracker(i,j,k,0) < 7)
       {
          // Loop over my neighbors
          for (int ipair = 1; ipair <= itracker(i,j,k,0); ipair++)
          {
               // (i_n,j_n,k_n) is in the nbhd of (i,j,k)
               int i_n = i + imap[itracker(i,j,k,ipair)];
               int j_n = j + jmap[itracker(i,j,k,ipair)];
               int k_n = k + kmap[itracker(i,j,k,ipair)];

               if (debug_print)
                   amrex::Print() << "WORKING ON CELL " << IntVect(i,j,k) << " and its nbor " << IntVect(i_n,j_n,k_n) << std::endl;

               // Loop over the neighbors of my neighbors
               // If any of these aren't already my neighbor, make them my neighbor
               int ipair_n = 1;
               while (ipair_n <= itracker(i_n,j_n,k_n,0))
               {
                    // amrex::Print() << " IPAIR_N IS " << ipair_n << std::endl;
                    // (i_nn,j_nn,k_nn) is in the nbhd of (i_n,j_n,k_n)
                    int i_nn = i_n + imap[itracker(i_n,j_n,k_n,ipair_n)];
                    int j_nn = j_n + jmap[itracker(i_n,j_n,k_n,ipair_n)];
                    int k_nn = k_n + kmap[itracker(i_n,j_n,k_n,ipair_n)];

                    bool found = false;

                    // Is this nbor of my nbor already my nbor (or me)??
                    for (int ipair_2 = 1; ipair_2 <= itracker(i,j,k,0); ipair_2++)
                    {
                    // amrex::Print() << " IPAIR_2 IS " << ipair_2 << std::endl;
                        // (i_n2,j_n2,k_n2) is in the nbhd of (i,j,k)
                        int i_n2 = i + imap[itracker(i,j,k,ipair_2)];
                        int j_n2 = j + jmap[itracker(i,j,k,ipair_2)];
                        int k_n2 = k + kmap[itracker(i,j,k,ipair_2)];
                        if ( (i_nn == i_n2 && j_nn == j_n2 && k_nn == k_n2) or 
                             (i_nn == i    && j_nn == j    && k_nn == k   ) )
                            found = true;
                    }
                    if (debug_print)
                    {
                        if (!found)
                            amrex::Print() << "DOING CELL " << IntVect(i,j,k) << " who has nbor " << IntVect(i_n,j_n,k_n) << 
                                           " who has nbor " << IntVect(i_nn,j_nn,k_nn) << " which was NOT found " << std::endl;
                        else
                           amrex::Print() << "DOING CELL " << IntVect(i,j,k) << " who has nbor " << IntVect(i_n,j_n,k_n) << 
                                            " who has nbor " << IntVect(i_nn,j_nn,k_nn) << " which was found " << std::endl;
                    }

                    if (!found)
                    {
                        // My neighbor had a neighbor I didn't know so adding it here
                        itracker(i,j,k,0) += 1;
                        int n_nbor = itracker(i,j,k,0);

                        if (k_nn == k) // The new neighbor is in the k-plane of the original cell
                        {
                            if (j_nn-j < 0) 
                               itracker(i,j,k,n_nbor) = (i_nn-i)+2; // short-cut for mapping onto 1,2 or 3
                            else if (j_nn-j > 0)
                               itracker(i,j,k,n_nbor) = (i_nn-i)+7; // short-cut for mapping onto 6,7, or 8
                            else if (i_nn-i > 0)
                               itracker(i,j,k,n_nbor) = 5;
                            else 
                               itracker(i,j,k,n_nbor) = 4;

                        } else if (k_nn == k-1) { // The new neighbor is in the (k-1)-plane of the original cell

                            if (j_nn-j < 0)
                               itracker(i,j,k,n_nbor) = (i_nn-i)+10; // short-cut for mapping onto  9, 10 or 11
                            else if (j_nn-j > 0)
                               itracker(i,j,k,n_nbor) = (i_nn-i)+16; // short-cut for mapping onto 15, 16 or 17
                            else 
                               itracker(i,j,k,n_nbor) = (i_nn-i)+13; // short-cut for mapping onto 12, 13 or 14

                        } else if (k_nn == k+1) { // The new neighbor is in the (k+1)-plane of the original cell
                            if (j_nn-j < 0)
                               itracker(i,j,k,n_nbor) = (i_nn-i)+19; // short-cut for mapping onto 18, 19 or 20
                            else if (j_nn-j > 0)
                               itracker(i,j,k,n_nbor) = (i_nn-i)+25; // short-cut for mapping onto 24, 25 or 26
                            else 
                               itracker(i,j,k,n_nbor) = (i_nn-i)+22; // short-cut for mapping onto 21, 22 or 23
                        }

                        if (debug_print)
                        {
                            amrex::Print() << "Adding " << IntVect(i_nn,j_nn,k_nn) << " to the nbor list of " << IntVect(i,j,k) << 
                                              " in component " << n_nbor << std::endl;
                            amrex::Print() << "Sanity check -- these should be the same: " << IntVect(i_nn,j_nn,k_nn) << " " << 
                                IntVect(i+imap[itracker(i,j,k,n_nbor)],j+jmap[itracker(i,j,k,n_nbor)],k+kmap[itracker(i,j,k,n_nbor)]) << std::endl;
                        }
                    }
                    ipair_n++; 
               }
          }
       }
    });
}
#endif
#endif
