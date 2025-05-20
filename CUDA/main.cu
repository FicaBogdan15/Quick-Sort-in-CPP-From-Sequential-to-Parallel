#include <cuda_runtime.h>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <vector>
#include <chrono>

__global__ void partitionPhase1(int* d_data,
                                int* d_bufSmall,
                                int* d_bufLarge,
                                int* d_countSmall,
                                int* d_countLarge,
                                int n,
                                int pivot)
{
    int tid = threadIdx.x;
    int quarter = (n+3) / 4;
    int start   = tid * quarter;
    int end     = min(start + quarter, n);

    int sc = 0, lc = 0;
    for(int i = start; i < end; i++) {
        int v = d_data[i];
        if (v < pivot) {
            d_bufSmall[tid * quarter + sc++] = v;
        } else {
            d_bufLarge[tid * quarter + lc++] = v;
        }
    }
    d_countSmall[tid] = sc;
    d_countLarge[tid] = lc;
}

__global__ void partitionPhase2(int* d_bufSmall,
                                int* d_bufLarge,
                                int* d_dataOut,
                                int* d_countSmall,
                                int* d_countLarge,
                                int n)
{
    int tid     = threadIdx.x;     // 0..3
    int quarter = (n+3) / 4;

    int totalSmall = 0;
    #pragma unroll
    for(int i = 0; i < 4; i++)
        totalSmall += d_countSmall[i];

    int sources[2], cnts[2], offset = 0;
    if (tid == 0) {
        // A preia < pivot din A(0) apoi din D(3)
        sources[0] = 0; sources[1] = 3;
        cnts[0]    = d_countSmall[0];
        cnts[1]    = d_countSmall[3];
        offset     = 0;
        for(int s = 0; s < 2; s++) {
            int src = sources[s], cnt = cnts[s];
            for(int i = 0; i < cnt; i++)
                d_dataOut[offset + i] = d_bufSmall[src * quarter + i];
            offset += cnt;
        }
    }
    else if (tid == 1) {
        // B preia < pivot din B(1) apoi din C(2)
        sources[0] = 1; sources[1] = 2;
        cnts[0]    = d_countSmall[1];
        cnts[1]    = d_countSmall[2];
        offset     = d_countSmall[0] + d_countSmall[3];
        for(int s = 0; s < 2; s++) {
            int src = sources[s], cnt = cnts[s];
            for(int i = 0; i < cnt; i++)
                d_dataOut[offset + i] = d_bufSmall[src * quarter + i];
            offset += cnt;
        }
    }
    else if (tid == 2) {
        // C preia >= pivot din C(2) apoi din B(1)
        sources[0] = 2; sources[1] = 1;
        cnts[0]    = d_countLarge[2];
        cnts[1]    = d_countLarge[1];
        offset     = totalSmall;
        for(int s = 0; s < 2; s++) {
            int src = sources[s], cnt = cnts[s];
            for(int i = 0; i < cnt; i++)
                d_dataOut[offset + i] = d_bufLarge[src * quarter + i];
            offset += cnt;
        }
    }
    else if (tid == 3) {
        // D preia >= pivot din D(3) apoi din A(0)
        sources[0] = 3; sources[1] = 0;
        cnts[0]    = d_countLarge[3];
        cnts[1]    = d_countLarge[0];
        offset     = totalSmall + d_countLarge[2] + d_countLarge[1];
        for(int s = 0; s < 2; s++) {
            int src = sources[s], cnt = cnts[s];
            for(int i = 0; i < cnt; i++)
                d_dataOut[offset + i] = d_bufLarge[src * quarter + i];
            offset += cnt;
        }
    }
}

void quickSortCUDA(int* h_data, int n)
{
    int *d_data, *d_bufSmall, *d_bufLarge, *d_dataOut;
    int *d_countSmall, *d_countLarge;
    cudaMalloc(&d_data,       n * sizeof(int));
    cudaMalloc(&d_bufSmall,   n * sizeof(int));
    cudaMalloc(&d_bufLarge,   n * sizeof(int));
    cudaMalloc(&d_dataOut,    n * sizeof(int));
    cudaMalloc(&d_countSmall, 4 * sizeof(int));
    cudaMalloc(&d_countLarge, 4 * sizeof(int));

    cudaMemcpy(d_data, h_data, n * sizeof(int), cudaMemcpyHostToDevice);

    int pivot = h_data[0];

    partitionPhase1<<<1,4>>>(d_data,
                             d_bufSmall,
                             d_bufLarge,
                             d_countSmall,
                             d_countLarge,
                             n,
                             pivot);
    cudaDeviceSynchronize();

    partitionPhase2<<<1,4>>>(d_bufSmall,
                             d_bufLarge,
                             d_dataOut,
                             d_countSmall,
                             d_countLarge,
                             n);
    cudaDeviceSynchronize();

    cudaMemcpy(h_data, d_dataOut, n * sizeof(int), cudaMemcpyDeviceToHost);

    int countSmall[4];
    cudaMemcpy(countSmall, d_countSmall, 4*sizeof(int), cudaMemcpyDeviceToHost);
    int totalSmall = countSmall[0] + countSmall[1]
                   + countSmall[2] + countSmall[3];

    std::sort(h_data,               h_data + totalSmall);
    std::sort(h_data + totalSmall,  h_data + n);

    cudaFree(d_data);
    cudaFree(d_bufSmall);
    cudaFree(d_bufLarge);
    cudaFree(d_dataOut);
    cudaFree(d_countSmall);
    cudaFree(d_countLarge);
}

int main()
{
    std::cout << "Opening input file...\n";
    std::ifstream fin(R"(C:\Users\rollo\OneDrive\Desktop\inputs\input12M.txt)");
    if (!fin) {
        std::cerr << "Error: could not open input file\n";
        return 1;
    }

    std::cout << "Creating output file...\n";
    std::ofstream fout("output.txt");
    if (!fout) {
        std::cerr << "Error: could not create output file\n";
        return 1;
    }

    std::cout << "Reading data into vector...\n";
    std::vector<int> h_vec;
    int x;
    while (fin >> x) {
        h_vec.push_back(x);
    }
    fin.close();
    std::cout << "Read " << h_vec.size() << " elements\n";


    int N = static_cast<int>(h_vec.size());
    int* h_arr = new int[N];
    for (int i = 0; i < N; i++)
        h_arr[i] = h_vec[i];

    std::cout << "Launching QuickSort on GPU (" << N << " elements)...\n";
    auto t0 = std::chrono::high_resolution_clock::now();
    quickSortCUDA(h_arr, N);
    auto t1 = std::chrono::high_resolution_clock::now();

    double elapsed_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    std::cout << "QuickSort CUDA completed in "
              << elapsed_ms << " ms\n";

    std::cout << "Writing sorted data to output.txt...\n";
    for (int i = 0; i < N; i++) {
        fout << h_arr[i];
        if (i + 1 < N) fout << ' ';
    }
    fout << '\n';
    fout.close();

    delete[] h_arr;
    std::cout << "Done.\n";
    return 0;
}
