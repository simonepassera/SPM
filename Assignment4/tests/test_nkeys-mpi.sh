#!/bin/sh

#SBATCH --partition=normal
#SBATCH -N 8
#SBATCH --ntasks-per-node=1
#SBATCH -o ./out/test_nkeys-mpi.out
#SBATCH -e ./err/test_nkeys-mpi.err


echo "-------- TEST nkeys-mpi 2 1000000 --------"

echo "TEST nkeys-mpi 2 1000000, 1 process 1 node"
for i in {1..10}
do
    mpirun --host node01 --report-bindings --oversubscribe --bind-to none -n 1 nkeys-mpi 2 1000000 0
done

echo "TEST nkeys-mpi 2 1000000, 2 process 1 node"
for i in {1..10}
do
    mpirun --host node01 --report-bindings --oversubscribe --bind-to none -n 2 nkeys-mpi 2 1000000 0
done

echo "TEST nkeys-mpi 2 1000000, 2 process 2 node"
for i in {1..10}
do
    mpirun --report-bindings --oversubscribe --bind-to none -n 2 nkeys-mpi 2 1000000 0
done

echo "-------- TEST nkeys-mpi 100 1000000 --------"

echo "TEST nkeys-mpi 100 1000000, 1 process 1 node"
for i in {1..10}
do
    mpirun --host node01 --report-bindings --oversubscribe --bind-to none -n 1 nkeys-mpi 100 1000000 0
done

echo "TEST nkeys-mpi 100 1000000, 2 process 1 node"
for i in {1..10}
do
    mpirun --host node01 --report-bindings --oversubscribe --bind-to none -n 2 nkeys-mpi 100 1000000 0
done

echo "TEST nkeys-mpi 100 1000000, 4 process 1 node"
for i in {1..10}
do
    mpirun --host node01 --report-bindings --oversubscribe --bind-to none -n 4 nkeys-mpi 100 1000000 0
done

echo "TEST nkeys-mpi 100 1000000, 8 process 1 node"
for i in {1..10}
do
    mpirun --host node01 --report-bindings --oversubscribe --bind-to none -n 8 nkeys-mpi 100 1000000 0
done

echo "TEST nkeys-mpi 100 1000000, 2 process 2 node"
for i in {1..10}
do
    mpirun --report-bindings --oversubscribe --bind-to none -n 2 nkeys-mpi 100 1000000 0
done

echo "TEST nkeys-mpi 100 1000000, 4 process 4 node"
for i in {1..10}
do
    mpirun --report-bindings --oversubscribe --bind-to none -n 4 nkeys-mpi 100 1000000 0
done

echo "TEST nkeys-mpi 100 1000000, 8 process 8 node"
for i in {1..10}
do
    mpirun --report-bindings --oversubscribe --bind-to none -n 8 nkeys-mpi 100 1000000 0
done

echo "TEST nkeys-mpi 100 1000000, 16 process 8 node"
for i in {1..10}
do
    mpirun --report-bindings --oversubscribe --bind-to none -n 16 nkeys-mpi 100 1000000 0
done

echo "TEST nkeys-mpi 100 1000000, 32 process 8 node"
for i in {1..10}
do
    mpirun --report-bindings --oversubscribe --bind-to none -n 32 nkeys-mpi 100 1000000 0
done

echo "TEST nkeys-mpi 100 1000000, 64 process 8 node"
for i in {1..10}
do
    mpirun --report-bindings --oversubscribe --bind-to none -n 64 nkeys-mpi 100 1000000 0
done