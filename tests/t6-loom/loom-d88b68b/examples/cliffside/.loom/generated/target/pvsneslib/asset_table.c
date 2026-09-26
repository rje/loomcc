#include <loom-pvsneslib/generated.h>

/* Adapter-private physical records; portable code sees handles only. */
#define LOOM_PVS_GENERATED_ASSET_CHUNK_COUNT ((loom_u16)17u)
#define LOOM_PVS_GENERATED_ASSET_CHUNK_STORAGE_COUNT 17u

const loom_u16 loom_pvs_generated_asset_chunk_count =
    LOOM_PVS_GENERATED_ASSET_CHUNK_COUNT;

const loom_u8 loom_pvs_generated_tick_frames = 2u;

const LoomPvsGeneratedAssetChunk
    loom_pvs_generated_asset_chunks[LOOM_PVS_GENERATED_ASSET_CHUNK_STORAGE_COUNT] = {
    { (LoomAssetHandle)0u, 0u, 32u, 0x8000u, 0x81u },
    { (LoomAssetHandle)1u, 0u, 1152u, 0x8020u, 0x81u },
    { (LoomAssetHandle)2u, 0u, 32u, 0x84a0u, 0x81u },
    { (LoomAssetHandle)3u, 0u, 1440u, 0x84c0u, 0x81u },
    { (LoomAssetHandle)4u, 0u, 32u, 0x8a60u, 0x81u },
    { (LoomAssetHandle)5u, 0u, 480u, 0x8a80u, 0x81u },
    { (LoomAssetHandle)6u, 0u, 1792u, 0x8c60u, 0x81u },
    { (LoomAssetHandle)7u, 0u, 32u, 0x9360u, 0x81u },
    { (LoomAssetHandle)8u, 0u, 96u, 0x9380u, 0x81u },
    { (LoomAssetHandle)9u, 0u, 4096u, 0x93e0u, 0x81u },
    { (LoomAssetHandle)10u, 0u, 4096u, 0xa3e0u, 0x81u },
    { (LoomAssetHandle)11u, 0u, 1056u, 0xb3e0u, 0x81u },
    { (LoomAssetHandle)12u, 0u, 16u, 0xb800u, 0x81u },
    { (LoomAssetHandle)13u, 0u, 2048u, 0xb810u, 0x81u },
    { (LoomAssetHandle)14u, 0u, 2048u, 0xc010u, 0x81u },
    { (LoomAssetHandle)15u, 0u, 2048u, 0xc810u, 0x81u },
    { (LoomAssetHandle)16u, 0u, 154u, 0xd010u, 0x81u },
};
