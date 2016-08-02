/*
 ------------------------------------------------------------------

 This file is part of the Open Ephys GUI
 Copyright (C) 2014 Open Eph

 ------------------------------------------------------------------

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.

 */

#include <H5Cpp.h>
#include "HDF5FileFormat.h"


using namespace H5;
using namespace NWBRecording;

//HDF5FileBase

HDF5FileBase::HDF5FileBase() : readyToOpen(false), opened(false)
{
    Exception::dontPrint();
};

HDF5FileBase::~HDF5FileBase()
{
    close();
}

bool HDF5FileBase::isOpen() const
{
    return opened;
}

bool HDF5FileBase::isReadyToOpen() const
{
	return readyToOpen;
}

int HDF5FileBase::open()
{
	return open(-1);
}

int HDF5FileBase::open(int nChans)
{
    if (!readyToOpen) return -1;
    if (File(getFileName()).existsAsFile())
        return open(false, nChans);
    else
        return open(true, nChans);

}

int HDF5FileBase::open(bool newfile, int nChans)
{
    int accFlags,ret=0;

    if (opened) return -1;

    try
    {
		FileAccPropList props = FileAccPropList::DEFAULT;
		if (nChans > 0)
		{
			props.setCache(0, 1667, 2 * 8 * 2 * CHUNK_XSIZE * nChans, 1);
			//std::cout << "opening HDF5 " << getFileName() << " with nchans: " << nChans << std::endl;
		}

        if (newfile) accFlags = H5F_ACC_TRUNC;
        else accFlags = H5F_ACC_RDWR;
        file = new H5File(getFileName().toUTF8(),accFlags,FileCreatPropList::DEFAULT,props);
        opened = true;
        if (newfile)
        {
            ret = createFileStructure();
        }

        if (ret)
        {
            file = nullptr;
            opened = false;
            std::cerr << "Error creating file structure" << std::endl;
        }


        return ret;
    }
    catch (FileIException error)
    {
        PROCESS_ERROR;
    }
}

void HDF5FileBase::close()
{
    file = nullptr;
    opened = false;
}

int HDF5FileBase::setAttribute(DataTypes type, const void* data, String path, String name)
{
    return setAttributeArray(type, data, 1, path, name);
}


int HDF5FileBase::setAttributeArray(DataTypes type, const void* data, int size, String path, String name)
{
    H5Location* loc;
    Group gloc;
    DataSet dloc;
    Attribute attr;
    DataType H5type;
    DataType origType;

    if (!opened) return -1;
    try
    {
        try
        {
            gloc = file->openGroup(path.toUTF8());
            loc = &gloc;
        }
        catch (FileIException error) //If there is no group with that path, try a dataset
        {
            dloc = file->openDataSet(path.toUTF8());
            loc = &dloc;
        }

        H5type = getH5Type(type);
        origType = getNativeType(type);

        if (size > 1)
        {
            hsize_t dims = size;
            H5type = ArrayType(H5type,1,&dims);
            origType = ArrayType(origType,1,&dims);
        }

        if (loc->attrExists(name.toUTF8()))
        {
            attr = loc->openAttribute(name.toUTF8());
        }
        else
        {
            DataSpace attr_dataspace(H5S_SCALAR);
            attr = loc->createAttribute(name.toUTF8(),H5type,attr_dataspace);
        }

        attr.write(origType,data);

    }
    catch (GroupIException error)
    {
        PROCESS_ERROR;
    }
    catch (AttributeIException error)
    {
        PROCESS_ERROR;
    }
    catch (DataSetIException error)
    {
        PROCESS_ERROR;
    }
    catch (FileIException error)
    {
        PROCESS_ERROR;
    }

    return 0;
}

int HDF5FileBase::setAttributeStr(String value, String path, String name)
{
    H5Location* loc;
    Group gloc;
    DataSet dloc;
    Attribute attr;

    if (!opened) return -1;

    StrType type(PredType::C_S1, value.length());
    try
    {
        try
        {
            gloc = file->openGroup(path.toUTF8());
            loc = &gloc;
        }
        catch (FileIException error) //If there is no group with that path, try a dataset
        {
            dloc = file->openDataSet(path.toUTF8());
            loc = &dloc;
        }

        if (loc->attrExists(name.toUTF8()))
        {
            //attr = loc.openAttribute(name.toUTF8());
            return -1; //string attributes cannot change size easily, better not allow overwritting.
        }
        else
        {
            DataSpace attr_dataspace(H5S_SCALAR);
            attr = loc->createAttribute(name.toUTF8(), type, attr_dataspace);
        }
        attr.write(type,value.toUTF8());

    }
    catch (GroupIException error)
    {
        PROCESS_ERROR;
    }
    catch (AttributeIException error)
    {
        PROCESS_ERROR;
    }
    catch (FileIException error)
    {
        PROCESS_ERROR;
    }
    catch (DataSetIException error)
    {
        PROCESS_ERROR;
    }


    return 0;
}

int HDF5FileBase::createGroup(String path)
{
    if (!opened) return -1;
    try
    {
        file->createGroup(path.toUTF8());
    }
    catch (FileIException error)
    {
        PROCESS_ERROR;
    }
    catch (GroupIException error)
    {
        PROCESS_ERROR;
    }
    return 0;
}

int HDF5FileBase::createGroupIfDoesNotExist(String path)
{
	if (!opened) return -1;
	try {
		file->childObjType(path.toRawUTF8());
	}
	catch (FileIException)
	{
		return createGroup(path);
	}
	return 0;
}

HDF5RecordingData* HDF5FileBase::getDataSet(String path)
{
    ScopedPointer<DataSet> data;

    if (!opened) return nullptr;

    try
    {
        data = new DataSet(file->openDataSet(path.toUTF8()));
        return new HDF5RecordingData(data.release());
    }
    catch (DataSetIException error)
    {
        error.printError();
        return nullptr;
    }
    catch (FileIException error)
    {
        error.printError();
        return nullptr;
    }
    catch (DataSpaceIException error)
    {
        error.printError();
        return nullptr;
    }
}

HDF5RecordingData* HDF5FileBase::createDataSet(DataTypes type, int sizeX, int chunkX, String path)
{
    int chunks[3] = {chunkX, 0, 0};
    return createDataSet(type,1,&sizeX,chunks,path);
}

HDF5RecordingData* HDF5FileBase::createDataSet(DataTypes type, int sizeX, int sizeY, int chunkX, String path)
{
    int size[2];
    int chunks[3] = {chunkX, 0, 0};
    size[0] = sizeX;
    size[1] = sizeY;
    return createDataSet(type,2,size,chunks,path);
}

HDF5RecordingData* HDF5FileBase::createDataSet(DataTypes type, int sizeX, int sizeY, int sizeZ, int chunkX, String path)
{
    int size[3];
    int chunks[3] = {chunkX, 0, 0};
    size[0] = sizeX;
    size[1] = sizeY;
    size[2] = sizeZ;
    return createDataSet(type,3,size,chunks,path);
}

HDF5RecordingData* HDF5FileBase::createDataSet(DataTypes type, int sizeX, int sizeY, int sizeZ, int chunkX, int chunkY, String path)
{
    int size[3];
    int chunks[3] = {chunkX, chunkY, 0};
    size[0] = sizeX;
    size[1] = sizeY;
    size[2] = sizeZ;
    return createDataSet(type,3,size,chunks,path);
}

HDF5RecordingData* HDF5FileBase::createDataSet(DataTypes type, int dimension, int* size, int* chunking, String path)
{
    ScopedPointer<DataSet> data;
    DSetCreatPropList prop;
    if (!opened) return nullptr;

    //Right now this classes don't support datasets with rank > 3.
    //If it's needed in the future we can extend them to be of generic rank
    if ((dimension > 3) || (dimension < 1)) return nullptr;

    DataType H5type = getH5Type(type);

    hsize_t dims[3], chunk_dims[3], max_dims[3];

    for (int i=0; i < dimension; i++)
    {
        dims[i] = size[i];
        if (chunking[i] > 0)
        {
            chunk_dims[i] = chunking[i];
            max_dims[i] = H5S_UNLIMITED;
        }
        else
        {
            chunk_dims[i] = size[i];
            max_dims[i] = size[i];
        }
    }

    try
    {
        DataSpace dSpace(dimension,dims,max_dims);
        prop.setChunk(dimension,chunk_dims);

        data = new DataSet(file->createDataSet(path.toUTF8(),H5type,dSpace,prop));
        return new HDF5RecordingData(data.release());
    }
    catch (DataSetIException error)
    {
        error.printError();
        return nullptr;
    }
    catch (FileIException error)
    {
        error.printError();
        return nullptr;
    }
    catch (DataSpaceIException error)
    {
        error.printError();
        return nullptr;
    }


}

H5::DataType HDF5FileBase::getNativeType(DataTypes type)
{
    switch (type)
    {
        case I8:
            return PredType::NATIVE_INT8;
            break;
        case I16:
            return PredType::NATIVE_INT16;
            break;
        case I32:
            return PredType::NATIVE_INT32;
            break;
        case I64:
            return PredType::NATIVE_INT64;
            break;
        case U8:
            return PredType::NATIVE_UINT8;
            break;
        case U16:
            return PredType::NATIVE_UINT16;
            break;
        case U32:
            return PredType::NATIVE_UINT32;
            break;
        case U64:
            return PredType::NATIVE_UINT64;
            break;
        case F32:
            return PredType::NATIVE_FLOAT;
            break;
		case F64:
			return PredType::NATIVE_DOUBLE;
			break;
        case STR:
            return StrType(PredType::C_S1,MAX_STR_SIZE);
            break;
    }
    return PredType::NATIVE_INT32;
}

H5::DataType HDF5FileBase::getH5Type(DataTypes type)
{
    switch (type)
    {
        case I8:
            return PredType::STD_I8LE;
            break;
        case I16:
            return PredType::STD_I16LE;
            break;
        case I32:
            return PredType::STD_I32LE;
            break;
        case I64:
            return PredType::STD_I64LE;
            break;
        case U8:
            return PredType::STD_U8LE;
            break;
        case U16:
            return PredType::STD_U16LE;
            break;
        case U32:
            return PredType::STD_U32LE;
            break;
        case U64:
            return PredType::STD_U64LE;
            break;
        case F32:
            return PredType::IEEE_F32LE;
            break;
		case F64:
			return PredType::IEEE_F64LE;
			break;
        case STR:
            return StrType(PredType::C_S1,MAX_STR_SIZE);
            break;
    }
    return PredType::STD_I32LE;
}

HDF5RecordingData::HDF5RecordingData(DataSet* data)
{
    DataSpace dSpace;
    DSetCreatPropList prop;
    ScopedPointer<DataSet> dataSet = data;
    hsize_t dims[3], chunk[3];

    dSpace = dataSet->getSpace();
    prop = dataSet->getCreatePlist();

    dimension = dSpace.getSimpleExtentDims(dims);
    prop.getChunk(dimension,chunk);

    this->size[0] = (int) dims[0];
    if (dimension > 1)
        this->size[1] = (int) dims[1];
    else
        this->size[1] = 1;
    if (dimension > 1)
        this->size[2] = (int) dims[2];
    else
        this->size[2] = 1;

    this->xChunkSize = (int) chunk[0];
    this->xPos = 0;
    this->dSet = dataSet;
    this->rowXPos.clear();
    this->rowXPos.insertMultiple(0,0,this->size[1]);
}

HDF5RecordingData::~HDF5RecordingData()
{
	//Safety
	dSet->flush(H5F_SCOPE_GLOBAL);
}
int HDF5RecordingData::writeDataBlock(int xDataSize, HDF5FileBase::DataTypes type, const void* data)
{
    return writeDataBlock(xDataSize,size[1],type,data);
}

int HDF5RecordingData::writeDataBlock(int xDataSize, int yDataSize, HDF5FileBase::DataTypes type, const void* data)
{
    hsize_t dim[3],offset[3];
    DataSpace fSpace;
    DataType nativeType;

    dim[2] = size[2];
    //only modify y size if new required size is larger than what we had.
    if (yDataSize > size[1])
        dim[1] = yDataSize;
    else
        dim[1] = size[1];
    dim[0] = xPos + xDataSize;
    try
    {
        //First be sure that we have enough space
        dSet->extend(dim);

        fSpace = dSet->getSpace();
        fSpace.getSimpleExtentDims(dim);
        size[0] = (int) dim[0];
        if (dimension > 1)
            size[1] = (int) dim[1];

        //Create memory space
        dim[0]=xDataSize;
        dim[1]=yDataSize;
        dim[2] = size[2];

        DataSpace mSpace(dimension,dim);
        //select where to write
        offset[0]=xPos;
        offset[1]=0;
        offset[2]=0;

        fSpace.selectHyperslab(H5S_SELECT_SET, dim, offset);

        nativeType = HDF5FileBase::getNativeType(type);

        dSet->write(data,nativeType,mSpace,fSpace);
        xPos += xDataSize;
    }
    catch (DataSetIException error)
    {
        PROCESS_ERROR;
    }
    catch (DataSpaceIException error)
    {
        PROCESS_ERROR;
    }
    return 0;
}


int HDF5RecordingData::writeDataRow(int yPos, int xDataSize, HDF5FileBase::DataTypes type, const void* data)
{
    hsize_t dim[2],offset[2];
    DataSpace fSpace;
    DataType nativeType;
    if (dimension > 2) return -4; //We're not going to write rows in datasets bigger than 2d.
    //    if (xDataSize != rowDataSize) return -2;
    if ((yPos < 0) || (yPos >= size[1])) return -2;

    try
    {
        if (rowXPos[yPos]+xDataSize > size[0])
        {
            dim[1] = size[1];
            dim[0] = rowXPos[yPos] + xDataSize;
            dSet->extend(dim);

            fSpace = dSet->getSpace();
            fSpace.getSimpleExtentDims(dim);
            size[0] = (int) dim[0];
        }
        if (rowXPos[yPos]+xDataSize > xPos)
        {
            xPos = rowXPos[yPos]+xDataSize;
        }

        dim[0] = xDataSize;
        dim[1] = 1;
        DataSpace mSpace(dimension,dim);

        fSpace = dSet->getSpace();
        offset[0] = rowXPos[yPos];
        offset[1] = yPos;
        fSpace.selectHyperslab(H5S_SELECT_SET, dim, offset);

        nativeType = HDF5FileBase::getNativeType(type);


        dSet->write(data,nativeType,mSpace,fSpace);

        rowXPos.set(yPos,rowXPos[yPos] + xDataSize);
    }
    catch (DataSetIException error)
    {
        PROCESS_ERROR;
    }
    catch (DataSpaceIException error)
    {
        PROCESS_ERROR;
    }
    catch (FileIException error)
    {
        PROCESS_ERROR;
    }
    return 0;
}

void HDF5RecordingData::getRowXPositions(Array<uint32>& rows)
{
    rows.clear();
    rows.addArray(rowXPos);
}

