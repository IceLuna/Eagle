#ifndef EG_SORT_COMMON
#define EG_SORT_COMMON

#ifdef __cplusplus

using uint = uint32_t;
using mat3 = glm::mat3;
using mat4 = glm::mat4;
using vec2 = glm::vec2;
using vec3 = glm::vec3;
using vec4 = glm::vec4;
using uvec2 = glm::uvec2;

#endif

struct SortData
{
    uint NumKeys;                              ///< The number of keys to sort
    int  NumBlocksPerThreadGroup;              ///< How many blocks of keys each thread group needs to process
    uint NumThreadGroups;                      ///< How many thread groups are being run concurrently for sort
    uint NumThreadGroupsWithAdditionalBlocks;  ///< How many thread groups need to process additional block data
    uint NumReduceThreadgroupPerBin;           ///< How many thread groups are summed together for each reduced bin entry
    uint NumScanValues;                        ///< How many values to perform scan prefix (+ add) on
    uint Shift;                                ///< What bits are being sorted (4 bit increments)
};

#define FFX_PARALLELSORT_SORT_BITS_PER_PASS	4
#define	FFX_PARALLELSORT_SORT_BIN_COUNT	(1 << FFX_PARALLELSORT_SORT_BITS_PER_PASS)
#define FFX_PARALLELSORT_ELEMENTS_PER_THREAD 4
#define FFX_PARALLELSORT_THREADGROUP_SIZE 128
#define FFX_DIVIDE_ROUNDING_UP(x, y) ((x + y - 1) / y)
#define FFX_PARALLELSORT_ITERATION_COUNT (32u / FFX_PARALLELSORT_SORT_BITS_PER_PASS)
#define FFX_PARALLELSORT_MAX_THREADGROUPS_TO_RUN    800

#endif
