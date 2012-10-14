/*
 *	Huffman.cpp
 *
 *	Huffman encoder/decoder.
 *	Coded by Joseph A. Marrero
 *	http://www.ManVsCode.com/
 *	December 16, 2007 - Merry Christmass!
 */
#include <stack>
#include <bitset>
#include <algorithm>
#include <climits>
#include <cassert>
#include "Huffman.h"
using namespace std;

namespace Encoding {


Huffman::Huffman( )
 : m_File(), m_FrequencyTable(), m_InBuffer(), m_OutBuffer(), m_Root(NULL)
{
}

Huffman::~Huffman( )
{
}

bool Huffman::open( const std::string &filename )
{ m_File.open( filename.c_str(), ios::in | ios::binary ); return !m_File.fail( ); }

bool Huffman::save( const std::string &outFile )
{
	m_File.open( outFile.c_str( ), ios::out | ios::binary );
	if( m_File.fail( ) )
		return false;

	for( Size i = 0; i < m_OutBuffer.size( ); i++ )
		m_File.write( (char *) &m_OutBuffer[ i ], sizeof(byte) );
	//m_File.write( &m_OutBuffer[ 0 ], m_OutBuffer.size( ) );
	close( );
	return true;
}

void Huffman::close( )
{ m_File.close( ); m_File.clear( ); }

bool Huffman::encode( const std::string &filename )
{
	if( !open( filename ) )
		return false;
	m_InBuffer.clear( );

	// build frequency table...
	while( !m_File.eof( ) )
	{
		byte currentByte = 0x00;
		m_File.read( (char *) &currentByte, sizeof(byte) );

		if( m_File.fail( ) ) break;

		m_InBuffer.push_back( currentByte );
		m_FrequencyTable[ currentByte ]++;
	}

	Size nFrequencyTableSize = m_FrequencyTable.size( ) * (sizeof(byte) + sizeof(Size)) + sizeof(Size);
	
#ifndef _DEBUG // ignore this check during debugging
	if( nFrequencyTableSize > m_InBuffer.size( ) )
		return false;
#endif

	LeafNodeTable leafNodeTable;
	
	if( !buildHuffmanTree( m_Root, m_FrequencyTable, &leafNodeTable ) )
		return false;

	stack<bool> bitSequenceStack;
	bitset<CHAR_BIT> partialByte;
	Size currentBit = 0;
	

	m_OutBuffer.clear( );

	// save file identifier...
	byte *pFileId = (byte *) &FILE_ID;
	for( int i = 0; i < 4; i++ )
		m_OutBuffer.push_back( (byte) *(pFileId + i) );

	
	Size nNumberOfBytesEncoded = m_InBuffer.size( );
	byte *p_nNumberOfBytesEncoded = (byte *) &nNumberOfBytesEncoded;

	// write the number of bytes compressed...
	for( int i = 0; i < sizeof(Size); i++ )
		m_OutBuffer.push_back( p_nNumberOfBytesEncoded[ i ] );

	saveFrequencyTable( m_OutBuffer, m_FrequencyTable );

	for( Size i = 0; i < m_InBuffer.size( ); i++ )
	{
		HuffNode *pLeafNode = leafNodeTable[ m_InBuffer[ i ] ];
		assert( pLeafNode != NULL );
		
		while( pLeafNode != m_Root )
		{
			bitSequenceStack.push( pLeafNode->parent->left == pLeafNode ? false : true );
			pLeafNode = pLeafNode->parent;
		}
		
		while( bitSequenceStack.size( ) > 0 )
		{
			partialByte.set( CHAR_BIT - (++currentBit), bitSequenceStack.top( ) );
			bitSequenceStack.pop( );

			if( currentBit >= 8 ) // write this full byte out 
			{
				Size ulByte = partialByte.to_ulong( );
				byte *p_ulByte = (byte *) &ulByte;
				m_OutBuffer.push_back( p_ulByte[ 0 ] );
				currentBit = 0;
				partialByte.reset( );
			}
		}
	}

	if( currentBit < 8 ) // write this full byte out 
	{
		Size ulByte = partialByte.to_ulong( );
		byte *p_ulByte = (byte *) &ulByte;
		m_OutBuffer.push_back( p_ulByte[ 0 ] );
		currentBit = 0;
	}
	
	destroyTree( m_Root );
	close( );
	return true;
}

bool Huffman::decode( const std::string &filename )
{
	if( !open( filename ) )
		return false;


	m_InBuffer.clear( );

	while( !m_File.eof( ) )
	{
		byte currentByte = 0x00;
		m_File.read( (char *) &currentByte, sizeof(byte) );
		
		if( m_File.fail( ) ) break;
		m_InBuffer.push_back( currentByte );
	}

	// verify the file identifier...
	byte *pFileId = (byte *) &FILE_ID;
	if( m_InBuffer[ 0 ] != pFileId[ 0 ] || 
		m_InBuffer[ 1 ] != pFileId[ 1 ] || 
		m_InBuffer[ 2 ] != pFileId[ 2 ] || 
		m_InBuffer[ 3 ] != pFileId[ 3 ] )
		return false; // not compressed with this code...

	Size nNumberOfBytesToBeDecoded = 0;
	// get the number of bytes to be decoded...
	memcpy( &nNumberOfBytesToBeDecoded, &m_InBuffer[ 4 ], sizeof(Size) );

	if( m_InBuffer.size( ) <= 0 )
		return false;
	if( !loadFrequencyTable( m_InBuffer, m_FrequencyTable, 4 + sizeof(Size) ) )
		return false;

	// build Huffman tree...
	if( !buildHuffmanTree( m_Root, m_FrequencyTable ) )
		return false;

	Size nStartPosition = 4 + sizeof(Size) + m_FrequencyTable.size( ) * (sizeof(byte) + sizeof(Size)) + sizeof(Size);
	bitset<CHAR_BIT> *pByteBits = new bitset<CHAR_BIT>( (Size) m_InBuffer[ nStartPosition ] );
	m_OutBuffer.clear( );
	Size nBit = 0;

	for( Size i = nStartPosition; i < m_InBuffer.size( ) && nNumberOfBytesToBeDecoded > 0; )
	{
		HuffNode *pNode = m_Root;
		
		while( !isLeaf( pNode ) )
		{
			if( nBit >= CHAR_BIT )
			{
				delete pByteBits;
				pByteBits =  new bitset<CHAR_BIT>( (Size) m_InBuffer[ ++i ] );
				nBit = 0;
			}

			if( !pByteBits->test( CHAR_BIT - (++nBit) ) )
				pNode = pNode->left;
			else
				pNode = pNode->right;
		}

		assert( pNode != NULL );
		m_OutBuffer.push_back( pNode->theByte );
		nNumberOfBytesToBeDecoded--;
	}

	delete pByteBits;
	destroyTree( m_Root );
	close( );
	return true;
}



Huffman::FrequencyTable Huffman::buildFrequencyTable( const byte *buffer, Size length )
{
	FrequencyTable frequencyTable;

	for( Size i = 0; i < length; i++ )
		frequencyTable[ buffer[ i ] ]++;
	return frequencyTable;
}

bool Huffman::buildHuffmanTree( HuffNode *&treeRoot, 
								FrequencyTable &frequencyTable,
								LeafNodeTable *leafNodeTable )
{
	if( frequencyTable.size( ) <= 0 ) return false;

	HuffCompare compare;
	vector<FrequencyAndHuffNode> frequencies;
	FrequencyTable::const_iterator itr;


	// bof build min heap...
	for( itr = frequencyTable.begin(); itr != frequencyTable.end(); ++itr )
		frequencies.push_back( FrequencyAndHuffNode( itr->second, 
							   new HuffNode( itr->first, itr->second, NULL, NULL, NULL, true ) ) );

	make_heap(frequencies.begin( ), frequencies.end( ), compare );
	// eof build min heap...

	// bof build huffman tree...
	HuffNode *pNode = NULL;

	while( frequencies.size( ) > 1 )
	{
		pop_heap( frequencies.begin( ), frequencies.end( ), compare );
		FrequencyAndHuffNode &freqAndHuffNode1 = frequencies[ frequencies.size( ) - 1 ];
		frequencies.pop_back( );

		pop_heap( frequencies.begin( ), frequencies.end( ), compare /* Make it a min heap */ );
		FrequencyAndHuffNode &freqAndHuffNode2 = frequencies[ frequencies.size( ) - 1 ];
		frequencies.pop_back( );

		if( leafNodeTable != NULL )
		{
			// add leaf nodes...
			if( freqAndHuffNode1.second->bContainsData )
				(*leafNodeTable)[ freqAndHuffNode1.second->theByte ] = freqAndHuffNode1.second;
			
			if( freqAndHuffNode2.second->bContainsData )
				(*leafNodeTable)[ freqAndHuffNode2.second->theByte ] = freqAndHuffNode2.second;
		}

		
		pNode = addTwoNodesToTree( freqAndHuffNode1, freqAndHuffNode2 );

		frequencies.push_back( FrequencyAndHuffNode( freqAndHuffNode1.first + freqAndHuffNode2.first, pNode ) );
		push_heap( frequencies.begin( ), frequencies.end(), compare );
	}
	
	treeRoot = pNode;
	// eof build huffman tree...
	return true;
}

Huffman::HuffNode *Huffman::addTwoNodesToTree( FrequencyAndHuffNode &f1, FrequencyAndHuffNode &f2 )
{
	// set up frequecy node 1
	//HuffNode *node1 = new HuffNode( f1.second->theByte, f1.first, NULL, NULL, NULL );
	HuffNode *node1 = f1.second;

	// set up frequecy node 2
	//HuffNode *node2 = new HuffNode( f2.second->theByte , f2.first, NULL, NULL, NULL );
	HuffNode *node2 = f2.second;

	// set up parent...
	HuffNode *parent = new HuffNode( NULL, node1->count + node2->count, node1, node2, NULL, false );
	node1->parent = parent;
	node2->parent = parent;


	return parent;
}




bool Huffman::encode( const Buffer &inBuffer, Buffer &outBuffer )
{
	if( inBuffer.size( ) <= 0 ) return false;

	FrequencyTable &frequencyTable = buildFrequencyTable( &inBuffer[ 0 ], inBuffer.size( ) );
	Size nFrequencyTableSize = frequencyTable.size( ) * (sizeof(byte) + sizeof(Size)) + sizeof(Size);
	
#ifndef _DEBUG
	if( nFrequencyTableSize > inBuffer.size( ) )
		return false;
#endif
	
	outBuffer.clear( );

	Size nNumberOfBytesEncoded = inBuffer.size( );
	byte *p_nNumberOfBytesEncoded = (byte *) &nNumberOfBytesEncoded;
	// write the number of bytes compressed...
	for( int i = 0; i < sizeof(Size); i++ )
		outBuffer.push_back( p_nNumberOfBytesEncoded[ i ] );

	LeafNodeTable leafNodeTable;
	HuffNode *root = NULL;
	
	if( !buildHuffmanTree( root, frequencyTable, &leafNodeTable ) )
		return false;

	stack<bool> bitSequenceStack;
	bitset<CHAR_BIT> partialByte;
	Size currentBit = 0;


	saveFrequencyTable( outBuffer, frequencyTable );
	
	for( Size i = 0; i < inBuffer.size( ); i++ )
	{
		HuffNode *pLeafNode = leafNodeTable[ inBuffer[ i ] ];
		assert( pLeafNode != NULL );
		
		while( pLeafNode != root )
		{
			bitSequenceStack.push( pLeafNode->parent->left == pLeafNode ? false : true );
			pLeafNode = pLeafNode->parent;
		}
		
		while( bitSequenceStack.size( ) > 0 )
		{
			partialByte.set( CHAR_BIT - (++currentBit), bitSequenceStack.top( ) );
			bitSequenceStack.pop( );

			if( currentBit >= 8 ) // write this full byte out 
			{
				Size ulByte = partialByte.to_ulong( );
				byte *p_ulByte = (byte *) &ulByte;
				outBuffer.push_back( p_ulByte[ 0 ] );
				currentBit = 0;
				partialByte.reset( );
			}
		}
	}


	if( currentBit < 8 ) // write this full byte out 
	{
		Size ulByte = partialByte.to_ulong( );
		byte *p_ulByte = (byte *) &ulByte;
		outBuffer.push_back( p_ulByte[ 0 ] );
		currentBit = 0;
	}
	
	destroyTree( root );
	return true;
}


bool Huffman::decode( const Buffer &inBuffer, Buffer &outBuffer )
{
	if( inBuffer.size( ) <= 0 ) return false;


	Size nNumberOfBytesToBeDecoded = 0;

	FrequencyTable frequencyTable;
	LeafNodeTable leafNodeTable;

	// get the number of bytes to be decoded...
	memcpy( &nNumberOfBytesToBeDecoded, &inBuffer[ 0 ], sizeof(Size) );

	// load frequency table...
	if( !loadFrequencyTable( inBuffer, frequencyTable, sizeof(Size) ) )
		return false; // loading frequency table failed...

	// build Huffman tree...
	HuffNode *tree = NULL;
	if( !buildHuffmanTree( tree, frequencyTable, NULL ) )
		return false;

	Size nStartPosition = sizeof(Size) + frequencyTable.size( ) * (sizeof(byte) + sizeof(Size)) + sizeof(Size);
	bitset<CHAR_BIT> *pByteBits = new bitset<CHAR_BIT>( (Size) inBuffer[ nStartPosition ] );
	outBuffer.clear( );
	Size nBit = 0;

	for( Size i = nStartPosition; i < inBuffer.size( ) && nNumberOfBytesToBeDecoded > 0; )
	{
		HuffNode *pNode = tree;
		
		while( !isLeaf( pNode ) )
		{
			if( nBit >= CHAR_BIT )
			{
				delete pByteBits;
				pByteBits =  new bitset<CHAR_BIT>( (Size) inBuffer[ ++i ] );
				nBit = 0;
			}

			if( !pByteBits->test( CHAR_BIT - (++nBit) ) )
				pNode = pNode->left;
			else
				pNode = pNode->right;
		}

		assert( pNode != NULL );
		outBuffer.push_back( pNode->theByte );
		nNumberOfBytesToBeDecoded--;
	}

	delete pByteBits;
	destroyTree( tree );
	return true;
}


void Huffman::saveFrequencyTable( Buffer &outBuffer, const FrequencyTable &frequencyTable )
{
	Size nSize = frequencyTable.size( );
	FrequencyTable::const_iterator itr;

	byte *p_nSize = (byte *) &nSize;
	
	// save the size...
	for( Size i = 0; i < sizeof(Size); i++ )
		outBuffer.push_back( p_nSize[ i ] );

	// save the table...
	for( itr = frequencyTable.begin( ); itr != frequencyTable.end( ); ++itr )
	{
		// save the byte...
		outBuffer.push_back( itr->first );

		// save the byte's frequency...
		byte *pFrequency = (byte *) &itr->second;
		for( Size i = 0; i < sizeof(Size); i++ )
			outBuffer.push_back( pFrequency[ i ] );
	}
}

bool Huffman::loadFrequencyTable( const Buffer &inBuffer, FrequencyTable &frequencyTable, Size nPositionOfFrequencyTable )
{
	Size nSize = 0;
	frequencyTable.clear( );

	if( inBuffer.size( ) < sizeof(Size) ) return false;

	Size totalTableSize = nSize * (sizeof(byte) + sizeof(Size));

	if( inBuffer.size( ) < totalTableSize ) return false;

	memcpy( &nSize, &inBuffer[ nPositionOfFrequencyTable ], sizeof(Size) );
	Size nPosition = nPositionOfFrequencyTable + sizeof(Size); // skip over the size we just read in...

	for( Size i = 0; i < nSize; i++ )
	{
		byte theByte = inBuffer[ nPosition++ ];
		Size frequency = 0;

		memcpy( &frequency, &inBuffer[ nPosition ], sizeof(Size) );
		nPosition += sizeof(Size);

		frequencyTable[ theByte ] = frequency;		
	}

	return true;
}

void Huffman::destroyTree( HuffNode *&tree )
{
	if( isLeaf( tree ) )
		delete tree;
	else 
	{
		destroyTree( tree->left );
		destroyTree( tree->right );
	}
}

} // end of namespace