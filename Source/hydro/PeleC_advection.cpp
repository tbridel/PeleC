#include "PeleC_Parameters.H" 
#include "PeleC_index_macros.H" 
#include "PeleC_EOS.H"
#include "PeleC_K.H" 

struct Real3{
    amrex::Real x;
    amrex::Real y; 
    amrex::Real z; 
};

AMREX_GPU_DEVICE
void PeleC_ctoprim(Box const& bx, FArrayBox const& ufab, FArrayBox& qfab, FArrayBox& qauxfab)
{

    const auto len = length(bx); 
    const auto lo  = lbound(bx); 
    const auto u   = ufab.view(lo); 
    const auto q   = qfab.view(lo); 
    const auto qa  = qauxfab.view(lo); 

    const amrex::Real smallr = 1.e-19; 
    const amrex::Real smallp = 1.e-10; 
    amrex::Real R = k_B*n_A; 
    EOS eos_state; 
    amrex::Real rho;
    amrex::Real rhoinv; 
    amrex::Real kineng; 
    Real3 v;
    int n, nq;  

    for         (int k = 0; k < len.z; ++k){
        for     (int j = 0; j < len.y; ++j){
            for (int i = 0; i < len.x; ++i){
                rho = u(i,j,k,URHO); 
                rhoinv = 1.e0/rho; 
                v.x  = u(i,j,k,UMX)*rhoinv, v.y =  u(i,j,k,UMY)*rhoinv, v.z = u(i,j,k,UMZ)*rhoinv; 
                kineng = 0.5e0*rho*(v.x*v.x + v.y*v.y + v.z*v.z); 
                q(i,j,k,QRHO) = rho; 
                q(i,j,k,QU) = v.x; 
                q(i,j,k,QV) = v.y;
                q(i,j,k,QW) = v.z; 
// Maybe #pragma unroll                 
                for(int ipassive = 0; ipassive < npassive; ++ipassive){
                    n = eos_state.upass_map(ipassive); 
                    nq = eos_state.qpass_map(ipassive); 
                    q(i,j,k,nq) = u(i,j,k,n)*rhoinv; 
                }


                eos_state.e = (u(i,j,k,UEDEN) - kineng)*rhoinv;
                eos_state.T = u(i,j,k,UTEMP); 
                eos_state.rho = rho; 
//TODO figure out massfrac and auxilary vars
#pragma unroll 
                for(int sp = 0; sp < NUM_SPECIES; ++sp)  eos_state.massfrac[sp] = q(i,j,k,sp+QFS);
#pragma unroll
                for(int ax = 0; ax < naux; ++ax) eos_state.aux[ax] = q(i,j,k,ax+QFX); 
// Call eos_re
                eos_state.eos_re(); 

                q(i,j,k,QTEMP) = eos_state.T; 
                q(i,j,k,QREINT) = eos_state.e * rho;
                q(i,j,k,QPRES) = eos_state.p; 
                q(i,j,k,QGAME) = eos_state.p/eos_state.e*rhoinv + 1.e0; 

//Auxilary Fab
//Need small and R should be in PeleC_Parameters.H
                qa(i,j,k,QDPDR)  = eos_state.dpdr_e; 
                qa(i,j,k,QDPDE)  = eos_state.dpde; 
                qa(i,j,k,QC)     = eos_state.cs; 
                qa(i,j,k,QCSML)  = amrex::max(small, small*qa(i,j,k,QC)); 
                qa(i,j,k,QRSPEC) = R/eos_state.wbar; 
            }
        }
    }

}