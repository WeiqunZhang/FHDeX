#!/bin/bash
#SBATCH --qos=debug
#SBATCH --time=10
#SBATCH --nodes=3
#SBATCH --tasks-per-node=32
#SBATCH --constraint=haswell

RUNDIR=RUN_SF
KMCSCR=in.kmc_eq
FHDSCR=inputs_fhd_SF

if [ -d $RUNDIR ]
then
  echo "ERROR: $RUNDIR already exists"
  exit
fi

mkdir $RUNDIR
cp $KMCSCR $RUNDIR
cp $FHDSCR $RUNDIR
cp $0 $RUNDIR
cp mpmd.conf $RUNDIR
cd $RUNDIR

echo "*** START: `date`"

srun -n80 -l --multi-prog ../mpmd.conf

echo "*** FINISH: `date`"
