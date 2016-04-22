.. _nersc:

Using at NERSC
====================

To use PyTOAST at NERSC, you need to have a Python3 software stack with all dependencies installed.  There are several ways to achieve this.


Temporary Solution
--------------------

For now, you can use a python 3.4 software stack built by Ted on edison::

    %> module use xxxxx
    %> module load pytoast

OR::

    %> module use xxxxx
    %> module load pytoast-deps

And then build and install your own copy of pytoast.


NERSC Intel Python / Anaconda
---------------------------------

We will be moving towards a solution using NERSC-supported python3 within a conda environment, with some extra packages.  TBD.
