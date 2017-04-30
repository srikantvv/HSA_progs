#include <CL/cl.h>
#include <malloc.h>

#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>

#ifdef KVM_SWITCH
#include "m5op.h"

void *m5_mem = (void*)0xffffc90000000000;
#endif

#define SUCCESS 0
#define FAILURE 1

#include "aes_benchmark.h"
// OpenCL datastructures
cl_context       context;
cl_device_id     *devices;
cl_command_queue commandQueue;
cl_program       program;
cl_kernel        kernel;

// Application datastructures
const int CACHE_LINE_SIZE = 64;
size_t grid_size = 512;
size_t work_group_size = 256;

// arguments
const int code_size = 5;
const char *code = "hello";
int *keys;
char *msg;
int chars_decoded = 0;

/* Setup OpenCL data structures */
int
setupOpenCL()
{
    cl_int status = 0;
    size_t deviceListSize;

    // 1. Get platform
    cl_uint numPlatforms;
    cl_platform_id platform = NULL;
    status = clGetPlatformIDs(0, NULL, &numPlatforms);
    if (status != CL_SUCCESS) {
        printf("Error: Getting Platforms. (clGetPlatformsIDs)\n");
        return FAILURE;
    }

    if (numPlatforms > 0) {
        cl_platform_id *platforms = new cl_platform_id[numPlatforms];
        status = clGetPlatformIDs(numPlatforms, platforms, NULL);
        if (status != CL_SUCCESS) {
            printf("Error: Getting Platform Ids. (clGetPlatformsIDs)\n");
            return FAILURE;
        }
        for (int i = 0; i < numPlatforms; ++i) {
            char pbuff[100];
            status = clGetPlatformInfo(platforms[i], CL_PLATFORM_VENDOR,
                                       sizeof(pbuff), pbuff, NULL);
            if (status != CL_SUCCESS) {
                printf("Error: Getting Platform Info.(clGetPlatformInfo)\n");
                return FAILURE;
            }
            printf("\n%s\n", pbuff);
            platform = platforms[i];
            if (!strcmp(pbuff, "Advanced Micro Devices, Inc.")) {
                break;
            }
        }
        delete platforms;
    }

    if (NULL == platform) {
        printf("NULL platform found so Exiting Application.\n");
        return FAILURE;
    }

    // 2. create context from platform
    cl_context_properties cps[3] =
        {CL_CONTEXT_PLATFORM, (cl_context_properties)platform, 0};
    context = clCreateContextFromType(cps, CL_DEVICE_TYPE_GPU, NULL, NULL,
                                      &status);
    if (status != CL_SUCCESS) {
        printf("Error: Creating Context. (clCreateContextFromType)\n");
        return FAILURE;
    }

    // 3. Get device info
    // 3a. Get # of devices
    status = clGetContextInfo(context, CL_CONTEXT_DEVICES, 0, NULL,
                              &deviceListSize);
    if (status != CL_SUCCESS) {
        printf("Error: Getting Context Info (1st clGetContextInfo)\n");
        return FAILURE;
    }

    // 3b. Get the device list data
    devices = (cl_device_id *)malloc(deviceListSize);
    if (devices == 0) {
        printf("Error: No devices found.\n");
        return FAILURE;
    }
    status = clGetContextInfo(context, CL_CONTEXT_DEVICES, deviceListSize,
                              devices, NULL);
    if (status != CL_SUCCESS) {
        printf("Error: Getting Context Info (2nd clGetContextInfo)\n");
        return FAILURE;
    }

    // 4. Create command queue for device
    commandQueue = clCreateCommandQueue(context, devices[0], 0, &status);
    if (status != CL_SUCCESS) {
        printf("Creating Command Queue. (clCreateCommandQueue)\n");
        return FAILURE;
    }

    const char *source = "dmmy text";

    size_t sourceSize[] = {strlen(source)};

    // 5b. Register the kernel with the runtime
    program = clCreateProgramWithSource(context, 1, &source, sourceSize,
                                        &status);
    if (status != CL_SUCCESS) {
      printf("Error: Loading kernel (clCreateProgramWithSource)\n");
      return FAILURE;
    }

    status = clBuildProgram(program, 1, devices, NULL, NULL, NULL);
    if (status != CL_SUCCESS) {
        printf("Error: Building kernel (clBuildProgram)\n");
        return FAILURE;
    }

    kernel = clCreateKernel(program, "read_kernel", &status);
    if (status != CL_SUCCESS) {
        printf("Error: Creating readKernel from program. (clCreateKernel)\n");
        return FAILURE;
    }

    return SUCCESS;
}


/* Run kernels */
int
runCLKernel(cl_kernel kernel)
{
    cl_int   status;
    cl_event event;

    cl_int ret;
  
    int num_blocks = text_length_ / 16;
    size_t global_dimensions[] = {static_cast<size_t>(num_blocks)};
    size_t local_dimensions[] = {64};
  
    ret = clSetKernelArg(kernel, 0, sizeof(cl_mem), &dev_ciphertext_);
  
    ret = clSetKernelArg(kernel, 1, sizeof(cl_mem), &dev_key_);
  
    ret = clEnqueueNDRangeKernel(commandQueue, kernel, 1, NULL, global_dimensions,
                                 local_dimensions, 0, NULL, &event);

    if (ret != CL_SUCCESS) {
        printf("Error: Enqueue failed. (clEnqueueNDRangeKernel)\n");
        return FAILURE;
    }

    // 3. Wait for the kernel
    status = clWaitForEvents(1, &event);
    if (status != CL_SUCCESS) {
        printf("Error: Waiting for kernel run to finish. (clWaitForEvents)\n");
        return FAILURE;
    }

    // 4. Cleanup
    status = clReleaseEvent(event);
    if (status != CL_SUCCESS) {
        printf("Error: Release event object. (clReleaseEvent)\n");
        return FAILURE;
    }

    return SUCCESS;
}


/* Release OpenCL resources (Context, Memory etc.) */
int
cleanupCL()
{
    cl_int status;
    status = clReleaseKernel(kernel);
    if (status != CL_SUCCESS) {
        printf("Error: In clReleaseKernel \n");
        return FAILURE;
    }
    status = clReleaseProgram(program);
    if (status != CL_SUCCESS) {
        printf("Error: In clReleaseProgram\n");
        return FAILURE;
    }
    status = clReleaseCommandQueue(commandQueue);
    if (status != CL_SUCCESS) {
        printf("Error: In clReleaseCommandQueue\n");
        return FAILURE;
    }
    status = clReleaseContext(context);
    if (status != CL_SUCCESS) {
        printf("Error: In clReleaseContext\n");
        return FAILURE;
    }

    return SUCCESS;
}

void Cleanup() {
  free(plaintext_);
  free(ciphertext_);
  cl_int ret;
  ret = clReleaseMemObject(dev_ciphertext_);
  ret = clReleaseMemObject(dev_key_);
}

void CopyDataToDevice() {
  cl_int ret;
  ret = clEnqueueWriteBuffer(commandQueue, dev_ciphertext_, CL_TRUE, 0,
                             text_length_, ciphertext_, 0, NULL, NULL);

  ret = clEnqueueWriteBuffer(commandQueue, dev_key_, CL_TRUE, 0,
                             kExpandedKeyLengthInBytes, expanded_key_, 0, NULL,
                             NULL);
}

void CopyDataBackFromDevice() {
  cl_int ret;

  ret = clEnqueueReadBuffer(commandQueue, dev_ciphertext_, CL_TRUE, 0,
                            text_length_, ciphertext_, 0, NULL, NULL);
}



void LoadPlaintext() {
  FILE *input_file = fopen(input_file_name_.c_str(), "r");
  if (!input_file) {
    fprintf(stderr, "Fail to open input file.\n");
    exit(1);
  }

  // Get the size of file
  fseek(input_file, 0L, SEEK_END);
  uint64_t file_size = ftell(input_file);
  fseek(input_file, 0L, SEEK_SET);

  // 2 char per hex number
  text_length_ = file_size / 2;
  plaintext_ = reinterpret_cast<uint8_t *>(malloc(text_length_));

  uint64_t index = 0;
  unsigned int byte;
  while (fscanf(input_file, "%02x", &byte) == 1) {
    plaintext_[index] = static_cast<uint8_t>(byte);
    index++;
  }

  fclose(input_file);
}

void LoadKey() {
  FILE *key_file = fopen(key_file_name_.c_str(), "r");
  if (!key_file) {
    fprintf(stderr, "Fail to open key file.\n");
    exit(1);
  }

  unsigned int byte;
  for (int i = 0; i < kKeyLengthInBytes; i++) {
    if (!fscanf(key_file, "%02x", &byte)) {
      fprintf(stderr, "Error in key file.\n");
      exit(1);
    }
    key_[i] = static_cast<uint8_t>(byte);
  }

  fclose(key_file);
}

void InitiateCiphertext(uint8_t **ciphertext) {
  *ciphertext = reinterpret_cast<uint8_t *>(malloc(text_length_));
  memcpy(*ciphertext, plaintext_, text_length_);
}

void DumpText(uint8_t *text) {
  printf("Text: ");
  for (uint64_t i = 0; i < text_length_; i++) {
    printf("%02x ", text[i]);
  }
  printf("\n");
}

void DumpKey() {
  printf("Key: ");
  for (int i = 0; i < kKeyLengthInBytes; i++) {
    printf("%02x ", key_[i]);
  }
  printf("\n");
}

void DumpExpandedKey() {
  printf("Expanded key: ");
  for (int i = 0; i < kExpandedKeyLengthInWords; i++) {
    printf("\nword[%d]: %08x ", i, expanded_key_[i]);
  }
  printf("\n");
}

void WordToBytes(uint32_t word, uint8_t *bytes) {
  bytes[0] = (word & 0xff000000) >> 24;
  bytes[1] = (word & 0x00ff0000) >> 16;
  bytes[2] = (word & 0x0000ff00) >> 8;
  bytes[3] = (word & 0x000000ff) >> 0;
}

uint32_t BytesToWord(uint8_t *bytes) {
  return (bytes[0] << 24) + (bytes[1] << 16) + (bytes[2] << 8) + bytes[3];
}

uint32_t RotateWord(uint32_t word) {
  uint32_t after_rotation;
  uint8_t bytes[5];
  WordToBytes(word, bytes);
  bytes[4] = bytes[0];

  after_rotation = BytesToWord(bytes + 1);
  return after_rotation;
}

uint32_t SubWord(uint32_t word) {
  uint32_t after_subword;
  uint8_t bytes[4];

  WordToBytes(word, bytes);

  for (int i = 0; i < 4; i++) {
    bytes[i] = s[bytes[i]];
  }

  after_subword = BytesToWord(bytes);
  return after_subword;
}

void ExpandKey() {
  for (int i = 0; i < kKeyLengthInWords; i++) {
    expanded_key_[i] = BytesToWord(key_ + 4 * i);
  }

  uint32_t temp;
  for (int i = kKeyLengthInWords; i < kExpandedKeyLengthInWords; i++) {
    temp = expanded_key_[i - 1];

    if (i % kKeyLengthInWords == 0) {
      uint32_t after_rotate_word = RotateWord(temp);
      uint32_t after_sub_word = SubWord(after_rotate_word);
      uint32_t rcon_word = Rcon[i / kKeyLengthInWords] << 24;
      temp = after_sub_word ^ rcon_word;
    } else if (i % kKeyLengthInWords == 4) {
      temp = SubWord(temp);
    }
    expanded_key_[i] = expanded_key_[i - kKeyLengthInWords] ^ temp;
  }
}

void Run() {
  ExpandKey();
  CopyDataToDevice();
    // Run the CL program
  runCLKernel(kernel);
  CopyDataBackFromDevice();
}

void Initialize() {
  LoadPlaintext();
  LoadKey();
  InitiateCiphertext(&ciphertext_);
  // Setup
  cl_int err;
  dev_ciphertext_ =
      clCreateBuffer(context, CL_MEM_READ_WRITE, text_length_, NULL, &err);

  dev_key_ = clCreateBuffer(context, CL_MEM_READ_ONLY,
                            kExpandedKeyLengthInBytes, NULL, &err);
}

int
main(int argc, char * argv[])
{
    if (argc < 3) {
        printf("Usage: ./aes <input file> < key file>\n");
        return 1;
    }

    SetInputFileName(argv[1]);
    SetKeyFileName(argv[2]);

    if (setupOpenCL() != SUCCESS) {
        return FAILURE;
    }
    // Initialize Host application
    Initialize();
    Run();
    // Releases OpenCL resources
    if (cleanupCL()!= SUCCESS) {
        return FAILURE;
    }
    Cleanup();
    return SUCCESS;
}


