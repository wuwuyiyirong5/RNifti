#ifndef _NIFTI_IMAGE_H_
#define _NIFTI_IMAGE_H_

#include <Rcpp.h>

#include "niftilib/nifti1_io.h"

// Thin wrapper around a C-style nifti_image struct that allows C++-style destruction
class NiftiImage
{
public:
    struct Block
    {
        const NiftiImage &image;
        const int dimension;
        const int index;
        
        Block (const NiftiImage &image, const int dimension, const int index)
            : image(image), dimension(dimension), index(index)
        {
            if (dimension != image->ndim)
                throw std::runtime_error("Blocks must be along the last dimension in the image");
        }
        
        Block & operator= (const NiftiImage &source)
        {
            if (source->datatype != image->datatype)
                throw std::runtime_error("New data does not have the same datatype as the target block");
            
            size_t blockSize = 1;
            for (int i=1; i<dimension; i++)
                blockSize *= image->dim[i];
            
            if (blockSize != source->nvox)
                throw std::runtime_error("New data does not have the same size as the target block");
            
            blockSize *= image->nbyper;
            memcpy(static_cast<char*>(image->data) + blockSize*index, source->data, blockSize);
            return *this;
        }
    };
    
    static std::map<std::string,short> DatatypeCodes;
    static std::map<std::string,short> buildDatatypeCodes ()
    {
        std::map<std::string,short> codes;
        codes["auto"] = DT_NONE;
        codes["none"] = DT_NONE;
        codes["unknown"] = DT_NONE;
        codes["uint8"] = DT_UINT8;
        codes["char"] = DT_UINT8;
        codes["int16"] = DT_INT16;
        codes["short"] = DT_INT16;
        codes["int32"] = DT_INT32;
        codes["int"] = DT_INT32;
        codes["float32"] = DT_FLOAT32;
        codes["float"] = DT_FLOAT32;
        codes["float64"] = DT_FLOAT64;
        codes["double"] = DT_FLOAT64;
        codes["int8"] = DT_INT8;
        codes["uint16"] = DT_UINT16;
        codes["uint32"] = DT_UINT32;
        codes["int64"] = DT_INT64;
        codes["uint64"] = DT_UINT64;
        return codes;
    }
    
    static short sexpTypeToNiftiType (const int sexpType)
    {
        if (sexpType == INTSXP || sexpType == LGLSXP)
            return DT_INT32;
        else if (sexpType == REALSXP)
            return DT_FLOAT64;
        else
            throw std::runtime_error("Array elements must be numeric");
    }
    
protected:
    nifti_image *image;
    bool persistent;
    
    void copy (nifti_image * const source);
    void copy (const NiftiImage &source);
    void copy (const Block &source);
    
    void initFromNiftiS4 (const Rcpp::RObject &object, const bool copyData = true);
    void initFromMriImage (const Rcpp::RObject &object, const bool copyData = true);
    void initFromList (const Rcpp::RObject &object);
    void initFromArray (const Rcpp::RObject &object, const bool copyData = true);
    
    void updatePixdim (const std::vector<float> &pixdim);
    void setPixunits (const std::vector<std::string> &pixunits);
    
public:
    NiftiImage ()
        : image(NULL), persistent(false) {}
    
    NiftiImage (const NiftiImage &source)
        : persistent(false)
    {
        this->copy(source);
#ifndef NDEBUG
        Rprintf("Creating NiftiImage with pointer %p\n", this->image);
#endif
    }
    
    NiftiImage (nifti_image * const image, const bool copy = false)
        : image(image), persistent(false)
    {
        if (copy)
            this->copy(image);
#ifndef NDEBUG
        Rprintf("Creating NiftiImage with pointer %p\n", this->image);
#endif
    }
    
    NiftiImage (const SEXP object, const bool readData = true);
    
    ~NiftiImage ()
    {
        if (!persistent)
        {
#ifndef NDEBUG
            Rprintf("Freeing NiftiImage with pointer %p\n", this->image);
#endif
            nifti_image_free(image);
        }
    }
    
    operator nifti_image* () const { return image; }
    
    nifti_image * operator-> () const { return image; }
    
    NiftiImage & operator= (const NiftiImage &source)
    {
        copy(source);
        return *this;
    }
    
    NiftiImage & operator= (const Block &source)
    {
        copy(source);
        return *this;
    }
    
    void setPersistence (const bool persistent)
    {
        this->persistent = persistent;
#ifndef NDEBUG
        if (persistent)
            Rprintf("Setting NiftiImage with pointer %p to be persistent\n", this->image);
#endif
    }
    
    bool isNull () const { return (image == NULL); }
    bool isPersistent () const { return persistent; }
    int nDims () const
    {
        if (image == NULL)
            return 0;
        else
            return image->ndim;
    }
    
    // Note that this function differs from its R equivalent in only dropping unitary dimensions after the last nonunitary one
    NiftiImage & drop ()
    {
        int ndim = image->ndim;
        while (image->dim[ndim] < 2)
            ndim--;
        image->dim[0] = image->ndim = ndim;
        
        return *this;
    }
    
    void rescale (const std::vector<float> &scales);
    void update (const SEXP array);
    
    mat44 xform (const bool preferQuaternion = true) const;
    
    const Block slice (const int i) const { return Block(*this, 3, i); }
    const Block volume (const int i) const { return Block(*this, 4, i); }
    
    Block slice (const int i) { return Block(*this, 3, i); }
    Block volume (const int i) { return Block(*this, 4, i); }
    
    void toFile (const std::string fileName, const short datatype) const;
    
    Rcpp::RObject toArray () const;
    Rcpp::RObject toPointer (const std::string label) const;
    Rcpp::RObject toArrayOrPointer (const bool internal, const std::string label) const;
    Rcpp::RObject headerToList () const;
};

inline void NiftiImage::copy (nifti_image * const source)
{
    if (source != NULL)
    {
        image = nifti_copy_nim_info(source);
        if (source->data != NULL)
        {
            size_t dataSize = nifti_get_volsize(source);
            image->data = calloc(1, dataSize);
            memcpy(image->data, source->data, dataSize);
        }
    }
}

inline void NiftiImage::copy (const NiftiImage &source)
{
    nifti_image *sourceStruct = source;
    copy(sourceStruct);
}

inline void NiftiImage::copy (const Block &source)
{
    nifti_image *sourceStruct = source.image;
    if (sourceStruct != NULL)
    {
        image = nifti_copy_nim_info(sourceStruct);
        image->dim[0] = source.image->dim[0] - 1;
        image->dim[source.dimension] = 1;
        image->pixdim[source.dimension] = 1.0;
        nifti_update_dims_from_array(image);
        
        if (sourceStruct->data != NULL)
        {
            size_t blockSize = nifti_get_volsize(image);
            image->data = calloc(1, blockSize);
            memcpy(image->data, static_cast<char*>(source.image->data) + blockSize*source.index, blockSize);
        }
    }
}

// Convert an S4 "nifti" object, as defined in the oro.nifti package, to a "nifti_image" struct
inline void NiftiImage::initFromNiftiS4 (const Rcpp::RObject &object, const bool copyData)
{
    nifti_1_header header;
    header.sizeof_hdr = 348;
    
    const std::vector<short> dims = object.slot("dim_");
    for (int i=0; i<8; i++)
        header.dim[i] = dims[i];
    
    header.intent_p1 = object.slot("intent_p1");
    header.intent_p2 = object.slot("intent_p2");
    header.intent_p3 = object.slot("intent_p3");
    header.intent_code = object.slot("intent_code");
    
    header.datatype = object.slot("datatype");
    header.bitpix = object.slot("bitpix");
    
    header.slice_start = object.slot("slice_start");
    header.slice_end = object.slot("slice_end");
    header.slice_code = Rcpp::as<int>(object.slot("slice_code"));
    header.slice_duration = object.slot("slice_duration");
    
    const std::vector<float> pixdims = object.slot("pixdim");
    for (int i=0; i<8; i++)
        header.pixdim[i] = pixdims[i];
    header.xyzt_units = Rcpp::as<int>(object.slot("xyzt_units"));
    
    header.vox_offset = object.slot("vox_offset");
    
    header.scl_slope = object.slot("scl_slope");
    header.scl_inter = object.slot("scl_inter");
    header.toffset = object.slot("toffset");
    
    header.cal_max = object.slot("cal_max");
    header.cal_min = object.slot("cal_min");
    header.glmax = header.glmin = 0;
    
    strncpy(header.descrip, Rcpp::as<std::string>(object.slot("descrip")).c_str(), 79);
    header.descrip[79] = '\0';
    strncpy(header.aux_file, Rcpp::as<std::string>(object.slot("aux_file")).c_str(), 23);
    header.aux_file[23] = '\0';
    strncpy(header.intent_name, Rcpp::as<std::string>(object.slot("intent_name")).c_str(), 15);
    header.intent_name[15] = '\0';
    strncpy(header.magic, Rcpp::as<std::string>(object.slot("magic")).c_str(), 3);
    header.magic[3] = '\0';
    
    header.qform_code = object.slot("qform_code");
    header.sform_code = object.slot("sform_code");
    
    header.quatern_b = object.slot("quatern_b");
    header.quatern_c = object.slot("quatern_c");
    header.quatern_d = object.slot("quatern_d");
    header.qoffset_x = object.slot("qoffset_x");
    header.qoffset_y = object.slot("qoffset_y");
    header.qoffset_z = object.slot("qoffset_z");
    
    const std::vector<float> srow_x = object.slot("srow_x");
    const std::vector<float> srow_y = object.slot("srow_y");
    const std::vector<float> srow_z = object.slot("srow_z");
    for (int i=0; i<4; i++)
    {
        header.srow_x[i] = srow_x[i];
        header.srow_y[i] = srow_y[i];
        header.srow_z[i] = srow_z[i];
    }
    
    if (header.datatype == DT_UINT8 || header.datatype == DT_INT16 || header.datatype == DT_INT32 || header.datatype == DT_INT8 || header.datatype == DT_UINT16 || header.datatype == DT_UINT32)
        header.datatype = DT_INT32;
    else if (header.datatype == DT_FLOAT32 || header.datatype == DT_FLOAT64)
        header.datatype = DT_FLOAT64;  // This assumes that sizeof(double) == 8
    else
        throw std::runtime_error("Data type is not supported");
    
    this->image = nifti_convert_nhdr2nim(header, NULL);
    
    const SEXP data = PROTECT(object.slot(".Data"));
    if (!copyData || Rf_length(data) <= 1)
        this->image->data = NULL;
    else
    {
        const size_t dataSize = nifti_get_volsize(this->image);
        this->image->data = calloc(1, dataSize);
        if (header.datatype == DT_INT32)
        {
            Rcpp::IntegerVector intData(data);
            std::copy(intData.begin(), intData.end(), static_cast<int32_t*>(this->image->data));
        }
        else
        {
            Rcpp::DoubleVector doubleData(data);
            std::copy(doubleData.begin(), doubleData.end(), static_cast<double*>(this->image->data));
        }
    }
    UNPROTECT(1);
}

inline void NiftiImage::initFromMriImage (const Rcpp::RObject &object, const bool copyData)
{
    Rcpp::Reference mriImage(object);
    Rcpp::Function getXform = mriImage.field("getXform");
    Rcpp::NumericMatrix xform = getXform();
    
    this->image = NULL;
    
    if (Rf_length(mriImage.field("tags")) > 0)
        initFromList(mriImage.field("tags"));
    
    Rcpp::RObject data = mriImage.field("data");
    if (data.inherits("SparseArray"))
    {
        Rcpp::Language call("Rcpp::as.array", data);
        data = call.eval();
    }
    
    const short datatype = (Rf_isNull(data) ? DT_INT32 : sexpTypeToNiftiType(data.sexp_type()));
    
    int dims[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
    const std::vector<int> dimVector = mriImage.field("imageDims");
    const int nDims = std::min(7, int(dimVector.size()));
    dims[0] = nDims;
    size_t nVoxels = 1;
    for (int i=0; i<nDims; i++)
    {
        dims[i+1] = dimVector[i];
        nVoxels *= dimVector[i];
    }
    
    if (this->image == NULL)
        this->image = nifti_make_new_nim(dims, datatype, FALSE);
    else
    {
        std::copy(dims, dims+8, this->image->dim);
        this->image->datatype = datatype;
        nifti_datatype_sizes(image->datatype, &image->nbyper, NULL);
    }
    
    if (copyData && !Rf_isNull(data))
    {
        // NB: nifti_get_volsize() will not be right here if there were tags
        const size_t dataSize = nVoxels * image->nbyper;
        this->image->data = calloc(1, dataSize);
        if (datatype == DT_INT32)
            memcpy(this->image->data, INTEGER(data), dataSize);
        else
            memcpy(this->image->data, REAL(data), dataSize);
    }
    else
        this->image->data = NULL;
    
    const std::vector<float> pixdimVector = mriImage.field("voxelDims");
    const int pixdimLength = pixdimVector.size();
    for (int i=0; i<std::min(pixdimLength,nDims); i++)
        this->image->pixdim[i+1] = std::abs(pixdimVector[i]);
    
    const std::vector<std::string> pixunitsVector = mriImage.field("voxelDimUnits");
    setPixunits(pixunitsVector);
    
    if (xform.rows() != 4 || xform.cols() != 4)
        this->image->qform_code = this->image->sform_code = 0;
    else
    {
        mat44 matrix;
        for (int i=0; i<4; i++)
        {
            for (int j=0; j<4; j++)
                matrix.m[i][j] = static_cast<float>(xform(i,j));
        }
        
        this->image->qto_xyz = matrix;
        this->image->qto_ijk = nifti_mat44_inverse(image->qto_xyz);
        nifti_mat44_to_quatern(image->qto_xyz, &image->quatern_b, &image->quatern_c, &image->quatern_d, &image->qoffset_x, &image->qoffset_y, &image->qoffset_z, NULL, NULL, NULL, &image->qfac);
        
        this->image->sto_xyz = matrix;
        this->image->sto_ijk = nifti_mat44_inverse(image->sto_xyz);
        
        this->image->qform_code = this->image->sform_code = 2;
    }
}

template <typename TargetType>
inline void copyIfPresent (const Rcpp::List &list, const std::set<std::string> names, const std::string &name, TargetType &target)
{
    if (names.count(name) == 1)
        target = Rcpp::as<TargetType>(list[name]);
}

// Special case for char, because Rcpp tries to be too clever and convert it to a string
template <>
inline void copyIfPresent (const Rcpp::List &list, const std::set<std::string> names, const std::string &name, char &target)
{
    if (names.count(name) == 1)
        target = static_cast<char>(Rcpp::as<int>(list[name]));
}

inline void NiftiImage::initFromList (const Rcpp::RObject &object)
{
    Rcpp::List list(object);
    nifti_1_header *header = nifti_make_new_header(NULL, DT_FLOAT64);
    
    const Rcpp::CharacterVector _names = list.names();
    std::set<std::string> names;
    for (Rcpp::CharacterVector::const_iterator it=_names.begin(); it!=_names.end(); it++)
        names.insert(Rcpp::as<std::string>(*it));
    
    copyIfPresent(list, names, "sizeof_hdr", header->sizeof_hdr);
    
    copyIfPresent(list, names, "dim_info", header->dim_info);
    if (names.count("dim") == 1)
    {
        std::vector<short> dim = list["dim"];
        for (int i=0; i<std::min(dim.size(),size_t(8)); i++)
            header->dim[i] = dim[i];
    }
    
    copyIfPresent(list, names, "intent_p1", header->intent_p1);
    copyIfPresent(list, names, "intent_p2", header->intent_p2);
    copyIfPresent(list, names, "intent_p3", header->intent_p3);
    copyIfPresent(list, names, "intent_code", header->intent_code);
    
    copyIfPresent(list, names, "datatype", header->datatype);
    copyIfPresent(list, names, "bitpix", header->bitpix);
    
    copyIfPresent(list, names, "slice_start", header->slice_start);
    if (names.count("pixdim") == 1)
    {
        std::vector<float> pixdim = list["pixdim"];
        for (int i=0; i<std::min(pixdim.size(),size_t(8)); i++)
            header->pixdim[i] = pixdim[i];
    }
    copyIfPresent(list, names, "vox_offset", header->vox_offset);
    copyIfPresent(list, names, "scl_slope", header->scl_slope);
    copyIfPresent(list, names, "scl_inter", header->scl_inter);
    copyIfPresent(list, names, "slice_end", header->slice_end);
    copyIfPresent(list, names, "slice_code", header->slice_code);
    copyIfPresent(list, names, "xyzt_units", header->xyzt_units);
    copyIfPresent(list, names, "cal_max", header->cal_max);
    copyIfPresent(list, names, "cal_min", header->cal_min);
    copyIfPresent(list, names, "slice_duration", header->slice_duration);
    copyIfPresent(list, names, "toffset", header->toffset);
    
    if (names.count("descrip") == 1)
        strcpy(header->descrip, Rcpp::as<std::string>(list["descrip"]).substr(0,79).c_str());
    if (names.count("aux_file") == 1)
        strcpy(header->aux_file, Rcpp::as<std::string>(list["aux_file"]).substr(0,23).c_str());
    
    copyIfPresent(list, names, "qform_code", header->qform_code);
    copyIfPresent(list, names, "sform_code", header->sform_code);
    copyIfPresent(list, names, "quatern_b", header->quatern_b);
    copyIfPresent(list, names, "quatern_c", header->quatern_c);
    copyIfPresent(list, names, "quatern_d", header->quatern_d);
    copyIfPresent(list, names, "qoffset_x", header->qoffset_x);
    copyIfPresent(list, names, "qoffset_y", header->qoffset_y);
    copyIfPresent(list, names, "qoffset_z", header->qoffset_z);
    
    if (names.count("srow_x") == 1)
    {
        std::vector<float> srow_x = list["srow_x"];
        for (int i=0; i<std::min(srow_x.size(),size_t(4)); i++)
            header->srow_x[i] = srow_x[i];
    }
    if (names.count("srow_y") == 1)
    {
        std::vector<float> srow_y = list["srow_y"];
        for (int i=0; i<std::min(srow_y.size(),size_t(4)); i++)
            header->srow_y[i] = srow_y[i];
    }
    if (names.count("srow_z") == 1)
    {
        std::vector<float> srow_z = list["srow_z"];
        for (int i=0; i<std::min(srow_z.size(),size_t(4)); i++)
            header->srow_z[i] = srow_z[i];
    }
    
    if (names.count("intent_name") == 1)
        strcpy(header->intent_name, Rcpp::as<std::string>(list["intent_name"]).substr(0,15).c_str());
    if (names.count("magic") == 1)
        strcpy(header->magic, Rcpp::as<std::string>(list["magic"]).substr(0,3).c_str());
    
    this->image = nifti_convert_nhdr2nim(*header, NULL);
    this->image->data = NULL;
    free(header);
}

inline void NiftiImage::initFromArray (const Rcpp::RObject &object, const bool copyData)
{
    int dims[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
    const std::vector<int> dimVector = object.attr("dim");
    
    const int nDims = std::min(7, int(dimVector.size()));
    dims[0] = nDims;
    for (int i=0; i<nDims; i++)
        dims[i+1] = dimVector[i];
    
    const short datatype = sexpTypeToNiftiType(object.sexp_type());
    this->image = nifti_make_new_nim(dims, datatype, int(copyData));
    
    if (copyData)
    {
        const size_t dataSize = nifti_get_volsize(image);
        if (datatype == DT_INT32)
            memcpy(this->image->data, INTEGER(object), dataSize);
        else
            memcpy(this->image->data, REAL(object), dataSize);
    }
    else
        this->image->data = NULL;
    
    if (object.hasAttribute("pixdim"))
    {
        const std::vector<float> pixdimVector = object.attr("pixdim");
        const int pixdimLength = pixdimVector.size();
        for (int i=0; i<std::min(pixdimLength,nDims); i++)
            this->image->pixdim[i+1] = pixdimVector[i];
    }
    
    if (object.hasAttribute("pixunits"))
    {
        const std::vector<std::string> pixunitsVector = object.attr("pixunits");
        setPixunits(pixunitsVector);
    }
}

inline NiftiImage::NiftiImage (const SEXP object, const bool readData)
    : persistent(false)
{
    Rcpp::RObject imageObject(object);
    
    if (Rf_isNull(object))
        this->image = NULL;
    else if (imageObject.hasAttribute(".nifti_image_ptr"))
    {
        Rcpp::XPtr<NiftiImage> imagePtr(SEXP(imageObject.attr(".nifti_image_ptr")));
        this->image = *imagePtr;
        this->persistent = true;
        
        if (imageObject.hasAttribute("dim"))
            update(object);
    }
    else if (Rf_isString(object))
    {
        std::string path = Rcpp::as<std::string>(object);
        this->image = nifti_image_read(path.c_str(), readData);
        if (this->image == NULL)
            throw std::runtime_error("Failed to read image");
    }
    else if (imageObject.inherits("nifti"))
        initFromNiftiS4(imageObject, readData);
    else if (imageObject.inherits("MriImage"))
        initFromMriImage(imageObject, readData);
    else if (Rf_isVectorList(object))
        initFromList(imageObject);
    else if (imageObject.hasAttribute("dim"))
        initFromArray(imageObject, readData);
    else
        throw std::runtime_error("Cannot convert object of class \"" + Rcpp::as<std::string>(imageObject.attr("class")) + "\" to a nifti_image");
    
    if (this->image != NULL)
        nifti_update_dims_from_array(this->image);
    
#ifndef NDEBUG
    Rprintf("Creating NiftiImage with pointer %p\n", this->image);
#endif
}

inline mat33 topLeftCorner (const mat44 &matrix)
{
    mat33 newMatrix;
    for (int i=0; i<3; i++)
    {
        for (int j=0; j<3; j++)
            newMatrix.m[i][j] = matrix.m[i][j];
    }
    
    return newMatrix;
}

inline void NiftiImage::updatePixdim (const std::vector<float> &pixdim)
{
    const int nDims = image->dim[0];
    const std::vector<float> origPixdim(image->pixdim+1, image->pixdim+4);
    
    for (int i=1; i<8; i++)
        image->pixdim[i] = 0.0;
    
    const int pixdimLength = pixdim.size();
    for (int i=0; i<std::min(pixdimLength,nDims); i++)
        image->pixdim[i+1] = pixdim[i];
    
    if (!std::equal(origPixdim.begin(), origPixdim.begin() + std::min(3,nDims), pixdim.begin()))
    {
        mat33 scaleMatrix;
        for (int i=0; i<3; i++)
        {
            for (int j=0; j<3; j++)
            {
                if (i != j)
                    scaleMatrix.m[i][j] = 0.0;
                else if (i >= nDims)
                    scaleMatrix.m[i][j] = 1.0;
                else
                    scaleMatrix.m[i][j] = pixdim[i] / origPixdim[i];
            }
        }
        
        if (image->qform_code > 0)
        {
            mat33 prod = nifti_mat33_mul(scaleMatrix, topLeftCorner(image->qto_xyz));
            for (int i=0; i<3; i++)
            {
                for (int j=0; j<3; j++)
                    image->qto_xyz.m[i][j] = prod.m[i][j];
            }
            image->qto_ijk = nifti_mat44_inverse(image->qto_xyz);
            nifti_mat44_to_quatern(image->qto_xyz, &image->quatern_b, &image->quatern_c, &image->quatern_d, &image->qoffset_x, &image->qoffset_y, &image->qoffset_z, NULL, NULL, NULL, &image->qfac);
        }
        
        if (image->sform_code > 0)
        {
            mat33 prod = nifti_mat33_mul(scaleMatrix, topLeftCorner(image->sto_xyz));
            for (int i=0; i<3; i++)
            {
                for (int j=0; j<3; j++)
                    image->sto_xyz.m[i][j] = prod.m[i][j];
            }
            image->sto_ijk = nifti_mat44_inverse(image->sto_xyz);
        }
    }
}

inline void NiftiImage::setPixunits (const std::vector<std::string> &pixunits)
{
    for (int i=0; i<pixunits.size(); i++)
    {
        if (pixunits[i] == "m")
            image->xyz_units = NIFTI_UNITS_METER;
        else if (pixunits[i] == "mm")
            image->xyz_units = NIFTI_UNITS_MM;
        else if (pixunits[i] == "um")
            image->xyz_units = NIFTI_UNITS_MICRON;
        else if (pixunits[i] == "s")
            image->time_units = NIFTI_UNITS_SEC;
        else if (pixunits[i] == "ms")
            image->time_units = NIFTI_UNITS_MSEC;
        else if (pixunits[i] == "us")
            image->time_units = NIFTI_UNITS_USEC;
        else if (pixunits[i] == "Hz")
            image->time_units = NIFTI_UNITS_HZ;
        else if (pixunits[i] == "ppm")
            image->time_units = NIFTI_UNITS_PPM;
        else if (pixunits[i] == "rad/s")
            image->time_units = NIFTI_UNITS_RADS;
    }
}

inline void NiftiImage::rescale (const std::vector<float> &scales)
{
    std::vector<float> pixdim(image->pixdim+1, image->pixdim+4);
    
    for (int i=0; i<std::min(3, int(scales.size())); i++)
    {
        if (scales[i] != 1.0)
        {
            pixdim[i] /= scales[i];
            image->dim[i+1] = static_cast<int>(std::floor(image->dim[i+1] * scales[i]));
        }
    }
    
    updatePixdim(pixdim);
    nifti_update_dims_from_array(image);
    
    // Data vector is now the wrong size, so drop it
    free(image->data);
}

inline void NiftiImage::update (const SEXP array)
{
    Rcpp::RObject object(array);
    if (!object.hasAttribute("dim"))
        return;
    
    for (int i=0; i<8; i++)
        image->dim[i] = 0;
    const std::vector<int> dimVector = object.attr("dim");
    
    const int nDims = std::min(7, int(dimVector.size()));
    image->dim[0] = nDims;
    for (int i=0; i<nDims; i++)
        image->dim[i+1] = dimVector[i];
    
    if (object.hasAttribute("pixdim"))
    {
        const std::vector<float> pixdimVector = object.attr("pixdim");
        updatePixdim(pixdimVector);
    }
    
    if (object.hasAttribute("pixunits"))
    {
        const std::vector<std::string> pixunitsVector = object.attr("pixunits");
        setPixunits(pixunitsVector);
    }
    
    // This NIfTI-1 library function clobbers dim[0] if the last dimension is unitary; we undo that here
    nifti_update_dims_from_array(image);
    image->dim[0] = image->ndim = nDims;
    
    image->datatype = NiftiImage::sexpTypeToNiftiType(object.sexp_type());
    nifti_datatype_sizes(image->datatype, &image->nbyper, NULL);
    
    free(image->data);
    
    const size_t dataSize = nifti_get_volsize(image);
    image->data = calloc(1, dataSize);
    if (image->datatype == DT_INT32)
        memcpy(image->data, INTEGER(object), dataSize);
    else
        memcpy(image->data, REAL(object), dataSize);
}

inline mat44 NiftiImage::xform (const bool preferQuaternion) const
{
    if (image == NULL)
    {
        mat44 matrix;
        for (int i=0; i<4; i++)
        {
            for (int j=0; j<4; j++)
                matrix.m[i][j] = 0.0;
        }
        return matrix;
    }
    else if (image->qform_code <= 0 && image->sform_code <= 0)
    {
        // No qform or sform so return RAS matrix (NB: other software may assume differently)
        mat44 matrix;
        for (int i=0; i<4; i++)
        {
            for (int j=0; j<4; j++)
                matrix.m[i][j] = (i==j ? 1.0 : 0.0);
        }
        return matrix;
    }
    else if ((preferQuaternion && image->qform_code > 0) || image->sform_code <= 0)
        return image->qto_xyz;
    else
        return image->sto_xyz;
}

template <typename SourceType, typename TargetType>
inline TargetType convertValue (SourceType value)
{
    return static_cast<TargetType>(value);
}

template <typename SourceType, typename TargetType>
inline void convertArray (const SourceType *source, const size_t length, TargetType *target)
{
    std::transform(source, source + length, target, convertValue<SourceType,TargetType>);
}

template <typename TargetType>
inline void changeDatatype (nifti_image *image, const short datatype)
{
    TargetType *data;
    const size_t dataSize = image->nvox * sizeof(TargetType);
    data = static_cast<TargetType *>(calloc(1, dataSize));
    
    switch (image->datatype)
    {
        case DT_UINT8:
        convertArray(static_cast<uint8_t *>(image->data), image->nvox, data);
        break;

        case DT_INT16:
        convertArray(static_cast<int16_t *>(image->data), image->nvox, data);
        break;

        case DT_INT32:
        convertArray(static_cast<int32_t *>(image->data), image->nvox, data);
        break;

        case DT_FLOAT32:
        convertArray(static_cast<float *>(image->data), image->nvox, data);
        break;

        case DT_FLOAT64:
        convertArray(static_cast<double *>(image->data), image->nvox, data);
        break;

        case DT_INT8:
        convertArray(static_cast<int8_t *>(image->data), image->nvox, data);
        break;

        case DT_UINT16:
        convertArray(static_cast<uint16_t *>(image->data), image->nvox, data);
        break;

        case DT_UINT32:
        convertArray(static_cast<uint32_t *>(image->data), image->nvox, data);
        break;

        case DT_INT64:
        convertArray(static_cast<int64_t *>(image->data), image->nvox, data);
        break;

        case DT_UINT64:
        convertArray(static_cast<uint64_t *>(image->data), image->nvox, data);
        break;

        default:
        throw std::runtime_error("Unsupported data type (" + std::string(nifti_datatype_string(image->datatype)) + ")");
    }
    
    free(image->data);
    image->data = data;
    image->datatype = datatype;
    nifti_datatype_sizes(datatype, &image->nbyper, &image->swapsize);
}

inline void NiftiImage::toFile (const std::string fileName, const short datatype) const
{
    // Copy the source image only if the datatype will be changed
    NiftiImage imageToWrite(image, datatype != DT_NONE);
    
    switch (datatype)
    {
        case DT_NONE:
        imageToWrite.setPersistence(true);
        break;
        
        case DT_UINT8:
        changeDatatype<uint8_t>(imageToWrite, datatype);
        break;

        case DT_INT16:
        changeDatatype<int16_t>(imageToWrite, datatype);
        break;

        case DT_INT32:
        changeDatatype<int32_t>(imageToWrite, datatype);
        break;

        case DT_FLOAT32:
        changeDatatype<float>(imageToWrite, datatype);
        break;

        case DT_FLOAT64:
        changeDatatype<double>(imageToWrite, datatype);
        break;

        case DT_INT8:
        changeDatatype<int8_t>(imageToWrite, datatype);
        break;

        case DT_UINT16:
        changeDatatype<uint16_t>(imageToWrite, datatype);
        break;

        case DT_UINT32:
        changeDatatype<uint32_t>(imageToWrite, datatype);
        break;

        case DT_INT64:
        changeDatatype<int64_t>(imageToWrite, datatype);
        break;

        case DT_UINT64:
        changeDatatype<uint64_t>(imageToWrite, datatype);
        break;

        default:
        throw std::runtime_error("Unsupported data type (" + std::string(nifti_datatype_string(datatype)) + ")");
    }
    
    const int status = nifti_set_filenames(imageToWrite, fileName.c_str(), false, true);
    if (status != 0)
        throw std::runtime_error("Failed to set filenames for NIfTI object");
    nifti_image_write(imageToWrite);
}

template <typename SourceType, int SexpType>
inline Rcpp::RObject imageDataToArray (const nifti_image *source)
{
    if (source == NULL)
        return Rcpp::RObject();
    else
    {
        SourceType *original = static_cast<SourceType *>(source->data);
        Rcpp::Vector<SexpType> array(static_cast<int>(source->nvox));
        
        if (SexpType == INTSXP || SexpType == LGLSXP)
            std::transform(original, original + source->nvox, array.begin(), convertValue<SourceType,int>);
        else if (SexpType == REALSXP)
            std::transform(original, original + source->nvox, array.begin(), convertValue<SourceType,double>);
        else
            throw std::runtime_error("Only numeric arrays can be created");
        
        return array;
    }
}

inline void finaliseNiftiImage (SEXP xptr)
{
    NiftiImage *object = (NiftiImage *) R_ExternalPtrAddr(xptr);
    object->setPersistence(false);
    delete object;
    R_ClearExternalPtr(xptr);
}

inline void addAttributes (Rcpp::RObject &object, nifti_image *source, const bool realDim = true)
{
    const int nDims = source->dim[0];
    Rcpp::IntegerVector dim(source->dim+1, source->dim+1+nDims);
    
    if (realDim)
        object.attr("dim") = dim;
    else
        object.attr("imagedim") = dim;
    
    Rcpp::DoubleVector pixdim(source->pixdim+1, source->pixdim+1+nDims);
    object.attr("pixdim") = pixdim;
    
    if (source->xyz_units == NIFTI_UNITS_UNKNOWN && source->time_units == NIFTI_UNITS_UNKNOWN)
        object.attr("pixunits") = "Unknown";
    else
    {
        Rcpp::CharacterVector pixunits(2);
        pixunits[0] = nifti_units_string(source->xyz_units);
        pixunits[1] = nifti_units_string(source->time_units);
        object.attr("pixunits") = pixunits;
    }
    
    NiftiImage *wrappedSource = new NiftiImage(source, true);
    wrappedSource->setPersistence(true);
    Rcpp::XPtr<NiftiImage> xptr(wrappedSource);
    R_RegisterCFinalizerEx(SEXP(xptr), &finaliseNiftiImage, FALSE);
    object.attr(".nifti_image_ptr") = xptr;
}

inline Rcpp::RObject NiftiImage::toArray () const
{
    Rcpp::RObject array;
    
    if (this->isNull())
        return array;
    
    switch (image->datatype)
    {
        case DT_UINT8:
        array = imageDataToArray<uint8_t,INTSXP>(image);
        break;
        
        case DT_INT16:
        array = imageDataToArray<int16_t,INTSXP>(image);
        break;
        
        case DT_INT32:
        array = imageDataToArray<int32_t,INTSXP>(image);
        break;
        
        case DT_FLOAT32:
        array = imageDataToArray<float,REALSXP>(image);
        break;
        
        case DT_FLOAT64:
        array = imageDataToArray<double,REALSXP>(image);
        break;
        
        case DT_INT8:
        array = imageDataToArray<int8_t,INTSXP>(image);
        break;
        
        case DT_UINT16:
        array = imageDataToArray<uint16_t,INTSXP>(image);
        break;
        
        case DT_UINT32:
        array = imageDataToArray<uint32_t,INTSXP>(image);
        break;
        
        case DT_INT64:
        array = imageDataToArray<int64_t,INTSXP>(image);
        break;
        
        case DT_UINT64:
        array = imageDataToArray<uint64_t,INTSXP>(image);
        break;
        
        default:
        throw std::runtime_error("Unsupported data type (" + std::string(nifti_datatype_string(image->datatype)) + ")");
    }
    
    addAttributes(array, image);
    const Rcpp::IntegerVector dim = array.attr("dim");
    array.attr("class") = Rcpp::CharacterVector::create("niftiImage");
    return array;
}

inline Rcpp::RObject NiftiImage::toPointer (const std::string label) const
{
    if (this->isNull())
        return Rcpp::RObject();
    else
    {
        Rcpp::RObject string = Rcpp::wrap(label);
        addAttributes(string, image, false);
        string.attr("class") = Rcpp::CharacterVector::create("internalImage", "niftiImage");
        return string;
    }
}

inline Rcpp::RObject NiftiImage::toArrayOrPointer (const bool internal, const std::string label) const
{
    return (internal ? toPointer(label) : toArray());
}

inline Rcpp::RObject NiftiImage::headerToList () const
{
    if (this->image == NULL)
        return Rcpp::RObject();
    
    nifti_1_header header = nifti_convert_nim2nhdr(this->image);
    Rcpp::List result;
    
    result["sizeof_hdr"] = header.sizeof_hdr;
    
    result["dim_info"] = int(header.dim_info);
    result["dim"] = std::vector<short>(header.dim, header.dim+8);
    
    result["intent_p1"] = header.intent_p1;
    result["intent_p2"] = header.intent_p2;
    result["intent_p3"] = header.intent_p3;
    result["intent_code"] = header.intent_code;
    
    result["datatype"] = header.datatype;
    result["bitpix"] = header.bitpix;
    
    result["slice_start"] = header.slice_start;
    result["pixdim"] = std::vector<float>(header.pixdim, header.pixdim+8);
    result["vox_offset"] = header.vox_offset;
    result["scl_slope"] = header.scl_slope;
    result["scl_inter"] = header.scl_inter;
    result["slice_end"] = header.slice_end;
    result["slice_code"] = int(header.slice_code);
    result["xyzt_units"] = int(header.xyzt_units);
    result["cal_max"] = header.cal_max;
    result["cal_min"] = header.cal_min;
    result["slice_duration"] = header.slice_duration;
    result["toffset"] = header.toffset;
    result["descrip"] = std::string(header.descrip, 80);
    result["aux_file"] = std::string(header.aux_file, 24);
    
    result["qform_code"] = header.qform_code;
    result["sform_code"] = header.sform_code;
    result["quatern_b"] = header.quatern_b;
    result["quatern_c"] = header.quatern_c;
    result["quatern_d"] = header.quatern_d;
    result["qoffset_x"] = header.qoffset_x;
    result["qoffset_y"] = header.qoffset_y;
    result["qoffset_z"] = header.qoffset_z;
    result["srow_x"] = std::vector<float>(header.srow_x, header.srow_x+4);
    result["srow_y"] = std::vector<float>(header.srow_y, header.srow_y+4);
    result["srow_z"] = std::vector<float>(header.srow_z, header.srow_z+4);
    
    result["intent_name"] = std::string(header.intent_name, 16);
    result["magic"] = std::string(header.magic, 4);
    
    result.attr("class") = Rcpp::CharacterVector::create("niftiHeader");
    
    return result;
}

#endif
