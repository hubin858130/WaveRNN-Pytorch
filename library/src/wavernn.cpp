/*
Copyright 2019 Eugene Ingerman

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include <stdio.h>
#include "wavernn.h"


TorchLayer *TorchLayer::loadNext(FILE *fd)
{
    TorchLayer::Header header;
    fread(&header, sizeof(TorchLayer::Header), 1, fd);

    printf("Loading: %s\n", header.name);

    switch( header.layerType ){

    case TorchLayer::Header::LayerType::Linear:
    {
        LinearLayer* linear = new LinearLayer();
        linear->loadNext(fd);
        addObject(linear);
        return linear;
    }
    break;

    case TorchLayer::Header::LayerType::GRU:
    {
        GRULayer* gru = new GRULayer();
        gru->loadNext(fd);
        addObject(gru);
        return gru;
    }
    break;

    case TorchLayer::Header::LayerType::Conv1d:
    case TorchLayer::Header::LayerType::Conv2d:
    case TorchLayer::Header::LayerType::BatchNorm1d:
    default:
        return nullptr;
    }
}


LinearLayer* LinearLayer::loadNext(FILE *fd)
{
    LinearLayer::Header header;
    fread( &header, sizeof(LinearLayer::Header), 1, fd);
    assert(header.elSize==4 or header.elSize==2);

    mat.read(fd, header.elSize); //read compressed array

    bias.resize(header.nRows);
    fread(bias.data(), header.elSize, header.nRows, fd);
}

Vectorf LinearLayer::operator()(const Vectorf &x)
{
    return mat*x;
}

GRULayer* GRULayer::loadNext(FILE *fd)
{
    GRULayer::Header header;
    fread( &header, sizeof(GRULayer::Header), 1, fd);
    assert(header.elSize==4 or header.elSize==2);

    nRows = header.nHidden;
    nCols = header.nInput;

    b_ir.resize(header.nHidden);
    b_iz.resize(header.nHidden);
    b_in.resize(header.nHidden);

    b_hr.resize(header.nHidden);
    b_hz.resize(header.nHidden);
    b_hn.resize(header.nHidden);


    W_ir.read( fd, header.elSize, header.nHidden, header.nInput);
    W_iz.read( fd, header.elSize, header.nHidden, header.nInput);
    W_in.read( fd, header.elSize, header.nHidden, header.nInput);

    W_hr.read( fd, header.elSize, header.nHidden, header.nHidden);
    W_hz.read( fd, header.elSize, header.nHidden, header.nHidden);
    W_hn.read( fd, header.elSize, header.nHidden, header.nHidden);

    fread(b_ir.data(), header.elSize, header.nHidden, fd);
    fread(b_iz.data(), header.elSize, header.nHidden, fd);
    fread(b_in.data(), header.elSize, header.nHidden, fd);

    fread(b_hr.data(), header.elSize, header.nHidden, fd);
    fread(b_hz.data(), header.elSize, header.nHidden, fd);
    fread(b_hn.data(), header.elSize, header.nHidden, fd);
}



Vectorf CompMatrix::operator*(const Vectorf &x)
{
    Vectorf y(nRows);
    assert(nCols == x.size());

    int indexPos = 0;
    int weightPos = 0;
    int row = 0;
    float sum = 0;

    while( row < nRows ){
        if( index[indexPos] != ROW_END_MARKER ){
            //TODO: vectorize this multiplication
            int col = SPARSE_GROUP_SIZE*index[indexPos];

            assert( x.size() < col+SPARSE_GROUP_SIZE );
            assert( weight.size() < weightPos+SPARSE_GROUP_SIZE);

            for(int i=0; i<SPARSE_GROUP_SIZE; ++i)
                sum += weight(weightPos++) * x(col+i);

        } else { //end of row. assign output and continue to the next row.
            assert( row < nRows );
            y(row) = sum;
            sum = 0.f;

            ++row;
            ++indexPos;
        }
    }
    return y;
}
