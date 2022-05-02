#include<iostream>
#include<fstream>
#include<mpi.h>

#ifdef RND
#  include<cstdlib>
#  include<unistd.h>
#endif

// set up load balancer parameters
#define MPI_CHUNK_MASTER 10  // max. number of jobs performed by master when all slaves are busy
#define MPI_CHUNK_SLAVE 40  // max. number of jobs performed by slave per MPI call

#define FLAG_START 17
#define FLAG_COLLECT 27
#define FLAG_TERMINATE 37

#define ARRAYDIM 1024
#define FILENAME_DATA "data.txt"

void search(int *a, int count, int toSearch, int rank)
{
    for (int i = 0; i < count; i++)
    {
        a[i] = (a[i] == toSearch) ? rank: -1;
    }
#ifdef RND
    sleep((rand() % 30) / 10);
#endif
}

int main(int argc, char *argv[])
{
#ifdef RND
    srand((unsigned int) time(NULL));
#endif

    // initialize MPI
    int mpi_size, mpi_rank;
    MPI_Status mpi_status;

    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &mpi_size);
    MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);

    int toSearch;

    // master's taking over
    if ( mpi_rank == 0 )
    {
        int array[ARRAYDIM];
        int array_size;

        // define and initialize support structure for load balancer
        struct MPI_data
        {
            int jobs_to_do;
            MPI_Request mpi_request;
        };

        MPI_data *mpi_data = new MPI_data[mpi_size];
        for (int i = 0; i < mpi_size; i++)
        {
            // jobs_to_do == 0 means that processor is not busy
            mpi_data[i].jobs_to_do = 0;
        }

        // serial program would start here

        // get the integer to be found
        if ( argc > 1 )
        {
            toSearch = atoi(argv[1]);
        }
        else
        {
            std::cerr << "An integer to be searched can be passed as command line argument." << std::endl;
            std::cerr << "Defaulting to 5." << std::endl;
            toSearch = 5;
        }

        // get the array to be searched
        std::ifstream in_data(FILENAME_DATA);
        if ( in_data.is_open() )
        {
            int i = 1;
            while ( !in_data.eof() )
            {
                in_data >> array[i];
                i++;
            }
            array_size = i - 2;
            in_data.close();
        }
        else
        {
            std::cerr << "Unable to open data file." << std::endl;
            array[0] = 4;
            array[1] = 5;
            array[2] = 6;
            array_size = 3;
        }

        // MPI_Bcast() should be blocking, but it's not ensured on all platforms
        MPI_Barrier(MPI_COMM_WORLD);

        // send data to slave processes
        MPI_Bcast(&toSearch, 1, MPI_INT, 0, MPI_COMM_WORLD);

        int master_sent = 0;
        int master_received = 0;
        int free_proc;

        // while we are not finished
        while ( master_received < array_size )
        {
            // if not all the jobs have been sent
            if ( master_sent < array_size )
            {
                // check for free processor
                // watch the countdown, let the master (rank==0) be free as long as possible
                for (int i = mpi_size - 1; i >= 0; i--)
                {
                    if ( mpi_data[i].jobs_to_do == 0 )
                    {
                        free_proc = i;
                        break;
                    }
                }
                // free slave processor found
                if ( free_proc > 0 )
                {
                    // determining the number of jobs to do
                    mpi_data[free_proc].jobs_to_do = (array_size - master_sent < MPI_CHUNK_SLAVE) ? array_size - master_sent: MPI_CHUNK_SLAVE;
                    // send jobs to particular slave
                    std::cout << "P " << mpi_rank << ": Sending " << mpi_data[free_proc].jobs_to_do << " pieces of data to free proc " << free_proc << std::endl;
                    MPI_Send(&array[master_sent+1], mpi_data[free_proc].jobs_to_do, MPI_INT, free_proc, FLAG_START, MPI_COMM_WORLD);
                    // receive results from particular slave
                    MPI_Irecv(&array[master_sent+1], mpi_data[free_proc].jobs_to_do, MPI_INT, free_proc, FLAG_COLLECT, MPI_COMM_WORLD, &(mpi_data[free_proc].mpi_request));
                    // job's been sent, not necessarily done
                    master_sent += mpi_data[free_proc].jobs_to_do;
                }
                // all slave processors are busy, master has to help them
                else
                {
                    mpi_data[free_proc].jobs_to_do = (array_size - master_sent < MPI_CHUNK_MASTER) ? array_size - master_sent: MPI_CHUNK_MASTER;
                    std::cout << "P " << free_proc << ": Doing " << mpi_data[free_proc].jobs_to_do << " pieces of data at proc " << free_proc << std::endl;
                    search(&array[master_sent+1], mpi_data[free_proc].jobs_to_do, toSearch, free_proc);
                    std::cout << "P " << free_proc << ": Done " << mpi_data[free_proc].jobs_to_do << " pieces of data at proc " << free_proc << std::endl;
                    master_sent += mpi_data[free_proc].jobs_to_do;
                    master_received += mpi_data[free_proc].jobs_to_do;
                    mpi_data[free_proc].jobs_to_do = 0;
                }
            }

            // test busy slaves whether they are finished
            int done;
            for (int i = 1; i < mpi_size; i++)
            {
                if ( mpi_data[i].jobs_to_do > 0 )
                {
                    done = 0;
                    MPI_Test(&(mpi_data[i].mpi_request), &done, &mpi_status);
                    // particular slave's finished
                    if ( done == 1 )
                    {
                        master_received += mpi_data[i].jobs_to_do;
                        std::cout << "P " << mpi_rank << ": Got " << mpi_data[i].jobs_to_do << " pieces of data from P " << i << std::endl;
                        mpi_data[i].jobs_to_do = 0;
                    }
                }
            }
        }
        // send ending flags to all slaves
        for (int i = 1; i < mpi_size; i++)
        {
            MPI_Send(&i, 1, MPI_INT, i, FLAG_TERMINATE, MPI_COMM_WORLD);
        }
        // follow up with serial code
        std::cout << std::endl;
        for (int i = 1; i <= array_size; i++)
        {
            if ( array[i] > -1 )
            {
                std::cout << "P " << array[i] << " found integer " << toSearch << " at position " << i << "." << std::endl;
            }
        }

        // deallocate support structure
        delete mpi_data;
        mpi_data = NULL;
    }

    // slaves start here
    if ( mpi_rank != 0 )
    {
        int array[MPI_CHUNK_SLAVE];
        MPI_Request mpi_request;

        // MPI_Bcast() should be blocking, but it's not ensured on all platforms
        MPI_Barrier(MPI_COMM_WORLD);
        // receive data from master
        MPI_Bcast(&toSearch, 1, MPI_INT, 0, MPI_COMM_WORLD);

        // do until a special ending flag breaks the cycle
        while ( true )
        {
            // receive the task from master
            MPI_Recv(array, MPI_CHUNK_SLAVE, MPI_INT, 0, MPI_ANY_TAG, MPI_COMM_WORLD, &mpi_status);
            // jobs came, do them
            if ( mpi_status.MPI_TAG == FLAG_START )
            {
                int really_received;
                MPI_Get_count(&mpi_status, MPI_INT, &really_received);

                search(array, really_received, toSearch, mpi_rank);

                // send results to master
                MPI_Isend(array, really_received, MPI_INT, 0, FLAG_COLLECT, MPI_COMM_WORLD, &(mpi_request));
            }
            // ending flag received
            else if ( mpi_status.MPI_TAG == FLAG_TERMINATE )
            {
                break;
            }
        }
    }

    // close MPI
    MPI_Finalize();
    return 0;
}
