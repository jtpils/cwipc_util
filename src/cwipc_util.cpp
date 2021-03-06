#include <cstddef>
#include <stdio.h>

#include <pcl/point_cloud.h>
#include <pcl/io/ply_io.h>


#ifdef WIN32
#define _CWIPC_UTIL_EXPORT __declspec(dllexport)
#else
#define _CWIPC_UTIL_EXPORT
#endif
#include "cwipc_util/api_pcl.h"
#include "cwipc_util/api.h"

struct dump_header {
    char hdr[4];
    uint32_t magic;
    uint64_t timestamp;
    size_t size;
};

class cwipc_impl : public cwipc {
protected:
    uint64_t m_timestamp;
    float m_cellsize;
    cwipc_pcl_pointcloud m_pc;
public:
    cwipc_impl() : m_timestamp(0), m_cellsize(0), m_pc(NULL) {}
    cwipc_impl(cwipc_pcl_pointcloud pc, uint64_t timestamp) : m_timestamp(timestamp), m_cellsize(0), m_pc(pc) {}

    ~cwipc_impl() {}

    int from_points(struct cwipc_point *pointData, size_t size, int npoint, uint64_t timestamp)
    {
        if (npoint * sizeof(struct cwipc_point) != size) {
            return -1;
        }
        m_timestamp = timestamp;
        cwipc_pcl_pointcloud pc = new_cwipc_pcl_pointcloud();
        for (int i=0; i<npoint; i++) {
            cwipc_pcl_point point;
            point.x = pointData[i].x;
            point.y = pointData[i].y;
            point.z = pointData[i].z;
            point.r = pointData[i].r;
            point.g = pointData[i].g;
            point.b = pointData[i].b;
            point.a = pointData[i].tile;
            pc->points.push_back(point);
        }
        m_pc = pc;
        return npoint;
    }

    void free() {
        m_pc = NULL;
    }
    
    uint64_t timestamp() {
        return m_timestamp;
    }
    
    float cellsize() {
        return m_cellsize;
    }
    
    void _set_cellsize(float cellsize) {
        m_cellsize = cellsize;
    }
    
    size_t get_uncompressed_size() {
        return m_pc->size() * sizeof(struct cwipc_point);
    }
    
    int copy_uncompressed(struct cwipc_point *pointData, size_t size) {
        if (size < m_pc->size() * sizeof(struct cwipc_point)) return -1;
        int npoint = m_pc->size();
        for (int i = 0; i < npoint; i++)
        {
            pointData[i].x = (*m_pc)[i].x;
            pointData[i].y = (*m_pc)[i].y;
            pointData[i].z = (*m_pc)[i].z;
            pointData[i].r = (*m_pc)[i].r;
            pointData[i].g = (*m_pc)[i].g;
            pointData[i].b = (*m_pc)[i].b;
            pointData[i].tile = (*m_pc)[i].a;
        }
        // iterate_over_octree();
        return npoint;
    }
    
    cwipc_pcl_pointcloud access_pcl_pointcloud() {
        return m_pc;
    }
};

cwipc *
cwipc_read(const char *filename, uint64_t timestamp, char **errorMessage, uint64_t apiVersion)
{
	if (apiVersion < CWIPC_API_VERSION_OLD || apiVersion > CWIPC_API_VERSION) {
		if (errorMessage) {
			*errorMessage = (char *)"cwipc_read: incorrect apiVersion";
		}
		return NULL;
	}
    cwipc_pcl_pointcloud pc = new_cwipc_pcl_pointcloud();
    pcl::PLYReader ply_reader;
    if (ply_reader.read(filename, *pc) < 0) {
        if (errorMessage) *errorMessage = (char *)"Error reading ply pointcloud";
        return NULL;
    }
    return new cwipc_impl(pc, timestamp);
}

int 
cwipc_write(const char *filename, cwipc *pointcloud, char **errorMessage)
{
    cwipc_pcl_pointcloud pc = pointcloud->access_pcl_pointcloud();
    if (pc == NULL) {
        if (errorMessage) *errorMessage = (char *)"Not yet implemented";
        return -1;
    }
    pcl::PLYWriter writer;
    int status = writer.write(filename, *pc);
    if (status < 0) {
        if (errorMessage) *errorMessage = (char *)"Saving of PLY file failed";
    }
	return status;
}

cwipc *
cwipc_read_debugdump(const char *filename, char **errorMessage, uint64_t apiVersion)
{
	if (apiVersion < CWIPC_API_VERSION_OLD || apiVersion > CWIPC_API_VERSION) {
		if (errorMessage) {
			*errorMessage = (char *)"cwipc_read_debugdump: incorrect apiVersion";
		}
		return NULL;
	}
    FILE *fp = fopen(filename, "rb");
    if (fp == NULL) {
        if (errorMessage) *errorMessage = (char *)"Cannot open pointcloud dumpfile";
        return NULL;
    }
    struct dump_header hdr;
    if (fread(&hdr, 1, sizeof(hdr), fp) != sizeof(hdr)) {
        if (errorMessage) *errorMessage = (char *)"Cannot read pointcloud dumpfile header";
        return NULL;
    }
    if (hdr.hdr[0] != 'c' || hdr.hdr[1] != 'p' || hdr.hdr[2] != 'c' || hdr.hdr[3] != 'd') {
        if (errorMessage) *errorMessage = (char *)"Pointcloud dumpfile header incorrect";
        return NULL;
    }
    if (hdr.magic < CWIPC_API_VERSION_OLD || hdr.magic > CWIPC_API_VERSION) {
        if (errorMessage) *errorMessage = (char *)"Pointcloud dumpfile version incorrect";
        return NULL;
    }
    uint64_t timestamp = hdr.timestamp;
    size_t dataSize = hdr.size;
    int npoint = dataSize / sizeof(cwipc_point);
    if (npoint*sizeof(cwipc_point) != dataSize) {
        if (errorMessage) *errorMessage = (char *)"Pointcloud dumpfile datasize inconsistent";
        return NULL;
    }
    cwipc_point* pointData = (cwipc_point *)malloc(dataSize);
    if (pointData == NULL) {
        if (errorMessage) *errorMessage = (char *)"Could not allocate memory for point data";
        return NULL;
    }
    if (fread(pointData, 1, dataSize, fp) != dataSize) {
        if (errorMessage) *errorMessage = (char *)"Could not read point data of correct size";
        return NULL;
    }
    cwipc_impl *pc = new cwipc_impl();
    pc->from_points(pointData, dataSize, npoint, timestamp);
    free(pointData);
	return pc;
}

int 
cwipc_write_debugdump(const char *filename, cwipc *pointcloud, char **errorMessage)
{
    size_t dataSize = pointcloud->get_uncompressed_size();
    struct cwipc_point *dataBuf = (struct cwipc_point *)malloc(dataSize);
    if (dataBuf == NULL) {
        if (errorMessage) *errorMessage = (char *)"Cannot allocate pointcloud memory";
        return -1;
    }
    int nPoint = pointcloud->copy_uncompressed(dataBuf, dataSize);
    if (nPoint < 0) {
        if (errorMessage) *errorMessage = (char *)"Cannot copy points from pointcloud";
        return -1;
    }
    FILE *fp = fopen(filename, "wb");
    if (fp == NULL) {
        if (errorMessage) *errorMessage = (char *)"Cannot create output file";
        return -1;
    }
    struct dump_header hdr = { {'c', 'p', 'c', 'd'}, CWIPC_API_VERSION, pointcloud->timestamp(), dataSize};
    fwrite(&hdr, sizeof(hdr), 1, fp);
    if (fwrite(dataBuf, sizeof(struct cwipc_point), nPoint, fp) != nPoint) {
        if (errorMessage) *errorMessage = (char *)"Write output file failed";
        return -1;
    }
    fclose(fp);
    return 0;
}

cwipc *
cwipc_from_pcl(cwipc_pcl_pointcloud pc, uint64_t timestamp, char **errorMessage, uint64_t apiVersion)
{
	if (apiVersion < CWIPC_API_VERSION_OLD || apiVersion > CWIPC_API_VERSION) {
		if (errorMessage) {
			*errorMessage = (char *)"cwipc_from_pcl: incorrect apiVersion";
		}
		return NULL;
	}
    return new cwipc_impl(pc, timestamp);
}


cwipc *
cwipc_from_points(cwipc_point* points, size_t size, int npoint, uint64_t timestamp, char **errorMessage, uint64_t apiVersion)
{
	if (apiVersion < CWIPC_API_VERSION_OLD || apiVersion > CWIPC_API_VERSION) {
		if (errorMessage) {
			*errorMessage = (char *)"cwipc_from_points: incorrect apiVersion";
		}
		return NULL;
	}
    cwipc_impl *rv = new cwipc_impl();
    if (rv->from_points(points, size, npoint, timestamp) < 0) {
        if (errorMessage) *errorMessage = (char *)"Cannot load points (size error?)";
        delete rv;
        return NULL;
    }
    return rv;
}

void cwipc_free(cwipc *pc)
{
    pc->free();
}

uint64_t
cwipc_timestamp(cwipc *pc)
{
    return pc->timestamp();
}

float
cwipc_cellsize(cwipc *pc)
{
    return pc->cellsize();
}

void
cwipc__set_cellsize(cwipc *pc, float cellsize)
{
    pc->_set_cellsize(cellsize);
}

size_t
cwipc_get_uncompressed_size(cwipc *pc)
{
    return pc->get_uncompressed_size();
}

int
cwipc_copy_uncompressed(cwipc *pc, struct cwipc_point *points, size_t size)
{
    return pc->copy_uncompressed(points, size);
}

cwipc* 
cwipc_source_get(cwipc_source *src)
{
    return src->get();
}

void 
cwipc_source_free(cwipc_source *src)
{
    src->free();
}

bool
cwipc_source_eof(cwipc_source *src)
{
	return src->eof();
}

bool 
cwipc_source_available(cwipc_source *src, bool wait)
{
	return src->available(wait);
}

int
cwipc_tiledsource_maxtile(cwipc_tiledsource *src)
{
	return src->maxtile();
}

bool
cwipc_tiledsource_get_tileinfo(cwipc_tiledsource *src, int tilenum, struct cwipc_tileinfo *tileinfo)
{
	return src->get_tileinfo(tilenum, tileinfo);
}

