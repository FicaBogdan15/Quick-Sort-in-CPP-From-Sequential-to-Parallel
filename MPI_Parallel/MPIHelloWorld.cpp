#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>

#define TOTAL_ELEMENTS 12500000
#define INPUT_PATH "C:\\Users\\rollo\\Desktop\\AN 3 Sem 2\\APD\\QuickSort_Paralel\\QuickSort_Paralel\\input12M.txt"
int cmp_int(const void* a, const void* b) {
    return (*(int*)a - *(int*)b);
}

int choose_pivot(int* arr, int n) {
    int first = arr[0];
    int middle = arr[n / 2];
    int last = arr[n - 1];
    if ((first <= middle && middle <= last) || (last <= middle && middle <= first))
        return middle;
    else if ((middle <= first && first <= last) || (last <= first && first <= middle))
        return first;
    else
        return last;
}

int* mpi_quicksort(int* arr, int local_n, MPI_Comm comm) {
    int size, rank;
    MPI_Comm_size(comm, &size);
    MPI_Comm_rank(comm, &rank);




    if (size == 1) {
        qsort(arr, local_n, sizeof(int), cmp_int);
        return arr;
    }

    int pivot;
    if (rank == 0) pivot = choose_pivot(arr, local_n);
    MPI_Bcast(&pivot, 1, MPI_INT, 0, comm);

    int* lo = (int*)malloc(local_n * sizeof(int));
    int* hi = (int*)malloc(local_n * sizeof(int));
    int nlo = 0, nhi = 0;
    for (int i = 0; i < local_n; i++) {
        if (arr[i] <= pivot) lo[nlo++] = arr[i];
        else                 hi[nhi++] = arr[i];
    }

    int* all_nlo = (int*)malloc(size * sizeof(int));
    int* all_nhi = (int*)malloc(size * sizeof(int));
    MPI_Allgather(&nlo, 1, MPI_INT, all_nlo, 1, MPI_INT, comm);
    MPI_Allgather(&nhi, 1, MPI_INT, all_nhi, 1, MPI_INT, comm);

    int total_lo = 0, total_hi = 0;
    int* disp_lo = (int*)malloc(size * sizeof(int));
    int* disp_hi = (int*)malloc(size * sizeof(int));
    for (int i = 0; i < size; i++) {
        disp_lo[i] = total_lo;
        total_lo += all_nlo[i];
        disp_hi[i] = total_hi;
        total_hi += all_nhi[i];
    }

    int* all_lo = NULL;
    int* all_hi = NULL;
    if (rank == 0) {
        all_lo = (int*)malloc(total_lo * sizeof(int));
        all_hi = (int*)malloc(total_hi * sizeof(int));
    }
    MPI_Gatherv(lo, nlo, MPI_INT, all_lo, all_nlo, disp_lo, MPI_INT, 0, comm);
    MPI_Gatherv(hi, nhi, MPI_INT, all_hi, all_nhi, disp_hi, MPI_INT, 0, comm);

    int half = size / 2;
    int color = (rank < half) ? 0 : 1;
    MPI_Comm subcomm;
    MPI_Comm_split(comm, color, rank, &subcomm);

    int* send_counts = (int*)malloc(size * sizeof(int));
    int* send_disp = (int*)malloc(size * sizeof(int));
    if (rank == 0) {
        int count = (color == 0) ? total_lo : total_hi;
        int group_size = (color == 0) ? half : size - half;
        int base = count / group_size;
        int rem = count % group_size;
        int off = 0;
        for (int i = 0; i < size; i++) {
            if ((color == 0 && i < half) || (color == 1 && i >= half)) {
                int idx = (color == 0) ? i : i - half;
                send_counts[i] = base + (idx < rem ? 1 : 0);
            }
            else {
                send_counts[i] = 0;
            }
            send_disp[i] = off;
            off += send_counts[i];
        }
    }
    MPI_Bcast(send_counts, size, MPI_INT, 0, comm);
    MPI_Bcast(send_disp, size, MPI_INT, 0, comm);

    int new_n = send_counts[rank];
    int* new_buf = (int*)malloc(new_n * sizeof(int));
    if (color == 0)
        MPI_Scatterv(all_lo, send_counts, send_disp, MPI_INT, new_buf, new_n, MPI_INT, 0, comm);
    else
        MPI_Scatterv(all_hi, send_counts, send_disp, MPI_INT, new_buf, new_n, MPI_INT, 0, comm);

    int* sorted = mpi_quicksort(new_buf, new_n, subcomm);

    MPI_Comm_free(&subcomm);
    free(lo);
    free(hi);
    free(all_nlo);
    free(all_nhi);
    free(disp_lo);
    free(disp_hi);
    if (rank == 0) {
        free(all_lo);
        free(all_hi);
    }
    free(send_counts);
    free(send_disp);
    return sorted;
}

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);
    int size, rank;
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    MPI_Barrier(MPI_COMM_WORLD);
    double t0 = MPI_Wtime();

    int n = TOTAL_ELEMENTS / size;
    int* local_data = (int*)malloc(n * sizeof(int));
    int* global_data = NULL;
    if (rank == 0) {
        global_data = (int*)malloc(TOTAL_ELEMENTS * sizeof(int));
        FILE* f = NULL;
        if (fopen_s(&f, INPUT_PATH, "r") != 0 || f == NULL) {
            perror("fopen_s input");
            MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
        }
        for (int i = 0; i < TOTAL_ELEMENTS; i++) {
            if (fscanf_s(f, "%d", &global_data[i]) != 1) {
                fprintf(stderr, "Error: file must contain at least %d numbers\n", TOTAL_ELEMENTS);
                MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
            }
        }
        fclose(f);
    }


    MPI_Scatter(global_data, n, MPI_INT,
        local_data, n, MPI_INT,
        0, MPI_COMM_WORLD);

    int* result = mpi_quicksort(local_data, n, MPI_COMM_WORLD);

    MPI_Gather(result, n, MPI_INT,
        global_data, n, MPI_INT,
        0, MPI_COMM_WORLD);

    if (rank == 0) {
        FILE* out;
        fopen_s(&out, "output.txt", "w");
        for (int i = 0; i < TOTAL_ELEMENTS; i++)
            fprintf(out, "%d\n", global_data[i]);
        fclose(out);
    }

    MPI_Barrier(MPI_COMM_WORLD);
    double t1 = MPI_Wtime();
    double elapsed = t1 - t0;
    double maxe;
    MPI_Reduce(&elapsed, &maxe, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        printf("Total execution time (including I/O and communication): %f seconds\n", maxe);
        free(global_data);
    }
    free(local_data);
    MPI_Finalize();
    return 0;
}

