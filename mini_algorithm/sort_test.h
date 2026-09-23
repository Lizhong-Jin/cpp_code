#pragma once

#include <iostream>

using namespace std;

void quickSort(vector<int>& nums, int left, int right) {
    if (left>=right) return;
    int target = nums[(left+right)>>1];
    int l=left, r=right;
    while (l<=r) {
        while (nums[l]<target) {
            l++;
        }
        while (nums[r]>target) {
            r--;
        }
        if (l<=r) {
            swap(nums[l], nums[r]);
            l++; r--;
        }
    }
    quickSort(nums, left, r);
    quickSort(nums, l, right);
}

void mergeSort(vector<int>& nums, vector<int>& temp, int left, int right) {
    if (left>=right) return;
    int mid = left + ((right-left)>>1);
    mergeSort(nums, temp, left, mid);
    mergeSort(nums, temp, mid+1, right);
    int i=left, j=mid+1, k=left;
    while (i<=mid && j<=right) {
        if (nums[i]<=nums[j]) {
            temp[k++] = nums[i++];
        }else {
            temp[k++] = nums[j++];
        }
    }
    while (i<=mid) {
        temp[k++] = nums[i++];
    }
    while (j<=right) {
        temp[k++] = nums[j++];
    }
    for (int k=left; k<=right; k++) {
        nums[k] = temp[k];
    }
}

void heapSort(vector<int>& nums) {
    int n=nums.size();
    for (int i=n/2-1; i>=0; i--) {
        int j=i, l_sub=i*2+1, r_sub=i*2+2, next=i;
        while (j<n/2) {
            if (l_sub<n&&nums[next]<nums[l_sub]) next=l_sub;
            if (r_sub<n&&nums[next]<nums[r_sub]) next=r_sub;
            if (next!=j) {
                swap(nums[j], nums[next]);
                j=next;
                l_sub=next*2+1, r_sub=next*2+2;
            }else break;
        }
    }
    while (n>0) {
        swap(nums[0], nums[n-1]);
        n--;
        int j=0, l_sub=1, r_sub=2, next=0;
        while (j<n/2) {
            if (l_sub<n&&nums[next]<nums[l_sub]) next=l_sub;
            if (r_sub<n&&nums[next]<nums[r_sub]) next=r_sub;
            if (next!=j) {
                swap(nums[j], nums[next]);
                j=next;
                l_sub=next*2+1, r_sub=next*2+2;
            }else break;
        }
    }
}

void sort_test() {}