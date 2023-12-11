#include "StochasticPC.H"

#include <AMReX_GpuContainers.H>
#include <AMReX_TracerParticle_mod_K.H>

using namespace amrex;

void
StochasticPC:: InitParticles (MultiFab& phi_fine)
{
    BL_PROFILE("StochasticPC::InitParticles");

    const int lev = 1;
    const Real* dx = Geom(lev).CellSize();
    const Real* plo = Geom(lev).ProbLo();

#if (AMREX_SPACEDIM == 2)
    const Real cell_vol = dx[0]*dx[1];
#else
    const Real cell_vol = dx[0]*dx[1]*dx[2];
#endif

#ifdef AMREX_USE_OMP
#pragma omp parallel if (amrex::Gpu::notInLaunchRegion())
#endif
    for (MFIter mfi(phi_fine); mfi.isValid(); ++mfi)
    {
        const Box& tile_box  = mfi.tilebox();

        const Array4<Real const>& phi_arr = phi_fine.const_array(mfi);

        // count the number of particles to create in each cell
        auto flat_index = FlatIndex(tile_box);
        Gpu::DeviceVector<unsigned int> counts(tile_box.numPts()+1, 0);
        unsigned int* pcount = counts.dataPtr();
        amrex::ParallelForRNG(tile_box,
        [=] AMREX_GPU_DEVICE (int i, int j, int k, amrex::RandomEngine const& engine) noexcept
        {
            Real rannum = amrex::Random(engine);
            int npart_in_cell = int(phi_arr(i,j,k,0)*cell_vol+rannum);
            pcount[flat_index(i, j, k)] += npart_in_cell;
        });

        // fill offsets
        Gpu::DeviceVector<unsigned int> offsets(tile_box.numPts()+1, 0);
        Gpu::exclusive_scan(counts.begin(), counts.end(), offsets.begin());

        // the last offset is the total number of particles to add
        unsigned int num_to_add;
        Gpu::copy(Gpu::deviceToHost, offsets.begin() + tile_box.numPts(), offsets.end(), &num_to_add);
        if (num_to_add == 0) continue;

        // Get the ids and cpu numbers to assign
        int my_cpu = ParallelDescriptor::MyProc();
        Long id_start;
#ifdef AMREX_USE_OMP
#pragma omp critical (init_particles_next_id)
#endif
        {
            id_start = ParticleType::NextID();
            ParticleType::NextID(id_start+num_to_add);
        }

        // resize particle storage
        auto& particles = GetParticles(lev);
        auto& particle_tile = particles[std::make_pair(mfi.index(), mfi.LocalTileIndex())];
        auto old_size = particle_tile.GetArrayOfStructs().size();
        auto new_size = old_size + num_to_add;
        particle_tile.resize(new_size);
        amrex::Print() << "INIT: NEW SIZE OF PARTICLES " << new_size << std::endl;

        // now fill in the data
        ParticleType* pstruct = particle_tile.GetArrayOfStructs()().data();
        unsigned int* poffset = offsets.dataPtr();
        amrex::ParallelForRNG(tile_box,
        [=] AMREX_GPU_DEVICE (int i, int j, int k, amrex::RandomEngine const& engine) noexcept
        {
            auto cellid = flat_index(i, j, k);

            auto start = poffset[cellid];
            auto stop = poffset[cellid+1];

            for (unsigned int ip = start; ip < stop; ++ip) {
                ParticleType& p = pstruct[ip];

#if (AMREX_SPACEDIM == 2)
                Real r[2] = {amrex::Random(engine), amrex::Random(engine)};
#elif (AMREX_SPACEDIM == 3)
                Real r[3] = {amrex::Random(engine), amrex::Random(engine), amrex::Random(engine)};
#endif
                AMREX_D_TERM( Real x = plo[0] + (i + r[0])*dx[0];,
                              Real y = plo[1] + (j + r[1])*dx[1];,
                              Real z = plo[2] + (k + r[2])*dx[2];);

                p.id()  = ip + id_start;
                p.cpu() = my_cpu;

                AMREX_D_TERM( p.pos(0) = x;,
                              p.pos(1) = y;,
                              p.pos(2) = z;);

                AMREX_D_TERM( p.rdata(RealIdx::xold) = x;,
                              p.rdata(RealIdx::yold) = y;,
                              p.rdata(RealIdx::zold) = z;);
            }
        });
    }
}

void
StochasticPC::AddParticles (MultiFab& phi_fine, BoxArray& ba_to_exclude)
{
    BL_PROFILE("StochasticPC::AddParticles");
    const int lev = 1;
    const Real* dx = Geom(lev).CellSize();
    const Real* plo = Geom(lev).ProbLo();

#if (AMREX_SPACEDIM == 2)
    Real cell_vol = dx[0]*dx[1];
#elif (AMREX_SPACEDIM == 3)
    Real cell_vol = dx[0]*dx[1]*dx[2];
#endif

    for (MFIter mfi(phi_fine); mfi.isValid(); ++mfi)
    {
        const Box& tile_box  = mfi.tilebox();

        if (!ba_to_exclude.contains(tile_box)) {

        const Array4<Real const>& phi_arr = phi_fine.const_array(mfi);

        Gpu::HostVector<ParticleType> host_particles;
        for (IntVect iv = tile_box.smallEnd(); iv <= tile_box.bigEnd(); tile_box.next(iv)) 
        {
              Real rannum = amrex::Random();
              int npart_in_cell = int(phi_arr(iv,0)*cell_vol+rannum);

              if (npart_in_cell > 0) {
#if 1
                 for (int npart = 0; npart < npart_in_cell; npart++)
                 {
#if (AMREX_SPACEDIM == 2)
                     Real r[2] = {amrex::Random(), amrex::Random()}; 
#elif (AMREX_SPACEDIM == 3)
                     Real r[3] = {amrex::Random(), amrex::Random(), amrex::Random()}; 
#endif
                     AMREX_D_TERM( Real x = plo[0] + (iv[0] + r[0])*dx[0];,
                                   Real y = plo[1] + (iv[1] + r[1])*dx[1];,
                                   Real z = plo[2] + (iv[2] + r[2])*dx[2];);

                     ParticleType p;
                     p.id()  = ParticleType::NextID();
                     p.cpu() = ParallelDescriptor::MyProc();
   
                     AMREX_D_TERM( p.pos(0) = x;,
                                   p.pos(1) = y;,
                                   p.pos(2) = z;);

                     AMREX_D_TERM( p.rdata(RealIdx::xold) = x;,
                                   p.rdata(RealIdx::yold) = y;,
                                   p.rdata(RealIdx::zold) = z;);

                     host_particles.push_back(p);
                 }
#else
                   amrex::ParallelForRNG( npart_in_cell,
                   [=] AMREX_GPU_DEVICE (int npart, RandomEngine const& engine) noexcept
                   {
#if (AMREX_SPACEDIM == 2)
                     Real r[2] = {amrex::Random(engine), amrex::Random(engine)}; 
#elif (AMREX_SPACEDIM == 3)
                     Real r[3] = {amrex::Random(engine), amrex::Random(engine), amrex::Random(engine)}; 
#endif
                     AMREX_D_TERM( Real x = plo[0] + (iv[0] + r[0])*dx[0];,
                                   Real y = plo[1] + (iv[1] + r[1])*dx[1];,
                                   Real z = plo[2] + (iv[2] + r[2])*dx[2];);

                     ParticleType p;
                     p.id()  = ParticleType::NextID();
                     p.cpu() = ParallelDescriptor::MyProc();
   
                     AMREX_D_TERM( p.pos(0) = x;,
                                   p.pos(1) = y;,
                                   p.pos(2) = z;);

                     AMREX_D_TERM( p.rdata(RealIdx::xold) = x;,
                                   p.rdata(RealIdx::yold) = y;,
                                   p.rdata(RealIdx::zold) = z;);

                     host_particles.push_back(p);
                 });
#endif
              } // npart_in_cell
        } // iv

        auto& particles = GetParticles(lev);
        auto& particle_tile = particles[std::make_pair(mfi.index(), mfi.LocalTileIndex())];
        auto old_size = particle_tile.GetArrayOfStructs().size();
        auto new_size = old_size + host_particles.size();
        particle_tile.resize(new_size);

        Gpu::copy(Gpu::hostToDevice,
                  host_particles.begin(),
                  host_particles.end(),
                  particle_tile.GetArrayOfStructs().begin() + old_size);
        } // not in ba_to_exclude
    } // mfi
}

void
StochasticPC::RemoveParticlesNotInBA (const BoxArray& ba_to_keep)
{
    BL_PROFILE("StochasticPC::RemoveParticles");
    const int lev = 1;

    for(ParIterType pti(*this, lev); pti.isValid(); ++pti)
    {
        auto& ptile = ParticlesAt(lev, pti);
        auto& aos  = ptile.GetArrayOfStructs();
        const int np = aos.numParticles();
        auto *pstruct = aos().data();

        if (!ba_to_keep.contains(pti.tilebox())) {
            amrex::ParallelFor(np, [=] AMREX_GPU_DEVICE (int i)
            {
                ParticleType& p = pstruct[i];
                p.id() = -1;
            });
}
    }
    Redistribute();
}

void
StochasticPC::RefluxFineToCrse (const BoxArray& ba_to_keep, MultiFab& phi_for_reflux) 
{
    BL_PROFILE("StochasticPC::RefluxFineToCrse");
    const int lev = 1;

    const auto dx = Geom(lev).CellSizeArray();

    for(ParIterType pti(*this, lev); pti.isValid(); ++pti)
    {
        auto& ptile = ParticlesAt(lev, pti);
        auto& aos  = ptile.GetArrayOfStructs();
        const int np = aos.numParticles();
        auto *pstruct = aos().data();

        if (!ba_to_keep.contains(pti.tilebox())) {

            Array4<Real> phi_arr = phi_for_reflux.array(pti.index());

            amrex::ParallelFor(np, [=] AMREX_GPU_DEVICE (int i)
            {
                ParticleType& p = pstruct[i];
    
#if (AMREX_SPACEDIM == 2)
                IntVect old_pos(static_cast<int>(p.rdata(RealIdx::xold)/dx[0]),static_cast<int>(p.rdata(RealIdx::yold)/dx[1]));
                IntVect new_pos(static_cast<int>(p.pos(0)              /dx[0]),static_cast<int>(p.pos(1)              /dx[1]));
#else
                IntVect old_pos(static_cast<int>(p.rdata(RealIdx::xold)/dx[0]),static_cast<int>(p.rdata(RealIdx::yold)/dx[1]),
                                static_cast<int>(p.rdata(RealIdx::zold)/dx[2]));
                IntVect new_pos(static_cast<int>(p.pos(0)              /dx[0]),static_cast<int>(p.pos(1)              /dx[1]),
                                static_cast<int>(p.pos(2)              /dx[2]));
#endif

                if ( ba_to_keep.contains(old_pos) && !ba_to_keep.contains(new_pos)) {
                   phi_arr(new_pos,0) += 1.;
                }
    
            });
        } // if not in ba_to_keep
    } // pti
}

void
StochasticPC::RefluxCrseToFine (const BoxArray& ba_to_keep, MultiFab& phi_for_reflux) 
{
    BL_PROFILE("StochasticPC::RefluxCrseToFine");
    const int lev = 1;
    const auto geom_lev = Geom(lev);
    const auto dx     = Geom(lev).CellSizeArray();

    for(ParIterType pti(*this, lev); pti.isValid(); ++pti)
    {
        auto const& gid   = pti.index();
        auto& ptile = ParticlesAt(lev, pti);
        auto& aos  = ptile.GetArrayOfStructs();
        const int np = aos.numParticles();
        auto *pstruct = aos().data();

        if (ba_to_keep.contains(pti.tilebox())) 
        {
            Array4<Real> phi_arr = phi_for_reflux.array(gid);

            amrex::ParallelFor(np, [=] AMREX_GPU_DEVICE (int i)
            {
                ParticleType& p = pstruct[i];
    
#if (AMREX_SPACEDIM == 2)
                IntVect old_pos(static_cast<int>(p.rdata(RealIdx::xold)/dx[0]),static_cast<int>(p.rdata(RealIdx::yold)/dx[1]));
                IntVect new_pos(static_cast<int>(p.pos(0)              /dx[0]),static_cast<int>(p.pos(1)              /dx[1]));
#else
                IntVect old_pos(static_cast<int>(p.rdata(RealIdx::xold)/dx[0]),static_cast<int>(p.rdata(RealIdx::yold)/dx[1]),
                                static_cast<int>(p.rdata(RealIdx::zold)/dx[2]));
                IntVect new_pos(static_cast<int>(p.pos(0)              /dx[0]),static_cast<int>(p.pos(1)              /dx[1]),
                                static_cast<int>(p.pos(2)              /dx[2]));
#endif

                // Make a box of the cell holding the particle in its previous position
                Box bx(old_pos,old_pos);
    
                if (!ba_to_keep.contains(old_pos) && ba_to_keep.contains(new_pos)) 
                {
                    if (Box(phi_arr).contains(old_pos)) {

                        phi_arr(old_pos,0) -= 1.;

                    } else {
#if (AMREX_SPACEDIM == 2)
                        Vector<IntVect> pshifts(9);
#else
                        Vector<IntVect> pshifts(27);
#endif
                        geom_lev.periodicShift(Box(phi_arr), bx, pshifts);
                        for (const auto& iv : pshifts) {
                            if (Box(phi_arr).contains(old_pos+iv)) {
                                phi_arr(old_pos+iv,0) -= 1.;
                                break;
                            }
                        } // pshifts
                    } // else
                } // if crossed the coarse-fine boundary
            }); // i
        } // if in ba_to_keep
    } // pti
}

void
StochasticPC::AdvectWithRandomWalk (int lev, Real dt)
{
    BL_PROFILE("StochasticPC::AdvectWithRandomWalk");
    const auto dx = Geom(lev).CellSizeArray();

    Real stddev = std::sqrt(dt);

    for(ParIterType pti(*this, lev); pti.isValid(); ++pti)
    {
        auto& ptile = ParticlesAt(lev, pti);
        auto& aos  = ptile.GetArrayOfStructs();
        const int np = aos.numParticles();
        auto *pstruct = aos().data();

        amrex::ParallelForRNG( np, [=] AMREX_GPU_DEVICE (int i, RandomEngine const& engine) noexcept
        {
            ParticleType& p = pstruct[i];
            AMREX_D_TERM( p.rdata(RealIdx::xold) = p.pos(0);,
                          p.rdata(RealIdx::yold) = p.pos(1);,
                          p.rdata(RealIdx::zold) = p.pos(2););

            AMREX_D_TERM( Real incx = amrex::RandomNormal(0.,stddev,engine);,
                          Real incy = amrex::RandomNormal(0.,stddev,engine);,
                          Real incz = amrex::RandomNormal(0.,stddev,engine););

            AMREX_D_TERM( incx = std::max(-dx[0], std::min( dx[0], incx));,
                          incy = std::max(-dx[1], std::min( dx[1], incy));,
                          incz = std::max(-dx[2], std::min( dx[2], incz)););

            AMREX_D_TERM( p.pos(0) += static_cast<ParticleReal> (incx);,
                          p.pos(1) += static_cast<ParticleReal> (incy);,
                          p.pos(2) += static_cast<ParticleReal> (incz););
        }); // np
    } // pti
}
