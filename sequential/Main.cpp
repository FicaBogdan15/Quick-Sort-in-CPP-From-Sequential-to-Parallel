#include <iostream>
#include <vector>
#include <fstream>
#include <chrono>
using namespace std;
using namespace std::chrono;


int parition(int v[], int low, int high) {

	int pivot = v[high]; 
	int swapMarker = low-1;

	for (int curIndex =low; curIndex < high; curIndex++) {
		
		if (v[curIndex] <= pivot) {
			swapMarker++;
			swap(v[swapMarker], v[curIndex]);

		}

	}
	swap(v[swapMarker+1], v[high]);
	return swapMarker + 1;


}

void quickSort(int v[],int low,int high) {

	if (low < high) {
		int pivot = parition(v, low, high); 
		quickSort(v, low, pivot-1); 
		quickSort(v,pivot+1, high);
	}
	

}


int main() {
	
	ifstream inFile("C:\\Users\\rollo\\Desktop\\AN 3 Sem 2\\APD\\Proiect\\DataGenerator\\DataGenerator\\input.txt"); 
	vector<int> numbers;
	int value;
	if (!inFile) {
		cerr << "Error: Could not open the file!" << endl;
		return 1;  
	}
	

	

	while (inFile >> value) {
		numbers.push_back(value);
		
	}
	

	int n = numbers.size();
	cout << n << endl;

	if (n == 0) {
		cerr << "Error: The file is empty!" << endl;
		return 1;
	}


	auto start = high_resolution_clock::now();

	quickSort(numbers.data(), 0, n - 1);

	auto stop = high_resolution_clock::now();
	auto duration = duration_cast<seconds>(stop-start);
	cout << "Sorting completed in: " << duration.count() << " s" << endl;
	
	ofstream outFile("output.txt");
	if (!outFile) {
		cerr << "Error: Could not create output.txt!" << endl;
		return 1;
	}

	for (int i = 0; i < n; i++) {
		outFile << numbers[i] << " ";
	}
	cout << "The sorted vector was placed in output.txt" << endl;
	

	return 0;
}
