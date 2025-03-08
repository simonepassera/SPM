#!/bin/sh

#SBATCH --partition=normal
#SBATCH -N 1
#SBATCH --ntasks-per-node=1
#SBATCH -o ./out/test_nkeys.out
#SBATCH -e ./err/test_nkeys.err

echo "TEST nkeys 2 1000000, 1 process 1 node"
for i in {1..10}
do
    ./nkeys 2 1000000 0
done

echo "TEST nkeys 100 1000000, 1 process 1 node"
for i in {1..10}
do
    ./nkeys 100 1000000 0
done
