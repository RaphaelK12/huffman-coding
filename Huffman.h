#pragma once
#ifndef _HUFFMAN_H_
#define _HUFFMAN_H_
/*
 *	Huffman.h
 *
 *	Huffman encoder/decoder.
 *	Coded by Joseph A. Marrero
 *	http://www.ManVsCode.com/
 *	December 16, 2007 - Merry Christmass!
 */
#include <fstream>
#include <vector>
#ifdef _TR1
#include <unordered_map> // This seems to cause it not to work! TO DO: Debug this crap
#else
#include <hash_map>
#endif


namespace Encoding {

class Huffman
{
  public:
	typedef unsigned char byte;
	typedef std::vector<byte> Buffer;
	typedef size_t Size;

  protected:
	static const int FILE_ID = 0xBEEFC0DE;
	struct HuffNode
	{
		byte theByte;
		Size count;
		HuffNode *left, *right, *parent;
		bool bContainsData;

		HuffNode( byte b, Size c, HuffNode *l, HuffNode *r, HuffNode *p, bool containsData = false )
		{ theByte = b; count = c; left = l; right = r; parent = p; bContainsData = containsData; }
	};

	typedef std::pair<Size, Huffman::HuffNode *> FrequencyAndHuffNode;
#ifdef _TR1
	typedef std::tr1::unordered_map<Huffman::byte, Size> FrequencyTable; // stdext::hash_map
	typedef std::tr1::unordered_map<Huffman::byte, HuffNode *> LeafNodeTable;
#else
	typedef stdext::hash_map<Huffman::byte, Size> FrequencyTable; 
	typedef stdext::hash_map<Huffman::byte, HuffNode *> LeafNodeTable;
#endif

	struct HuffCompare
	{
		bool operator()( const FrequencyAndHuffNode &n1, const FrequencyAndHuffNode &n2 ) const
		{ return n1.first > n2.first; }
	};



	std::fstream m_File;
	FrequencyTable m_FrequencyTable;
	
	//stdext::hash_map<byte, byte> m_ByteToCodeTable;
	Buffer m_InBuffer;
	Buffer m_OutBuffer;
	HuffNode *m_Root;

	/* Hidden Methods */
	static FrequencyTable buildFrequencyTable( const byte *buffer, Size length );
	static bool buildHuffmanTree( HuffNode *&treeRoot, 
								 FrequencyTable &frequencyTable,
								 LeafNodeTable *leafNodeTable = NULL );
	static HuffNode *addTwoNodesToTree( FrequencyAndHuffNode &f1, FrequencyAndHuffNode &f2 );
	static bool isLeaf( HuffNode *pNode ) { return pNode->right == NULL && pNode->left == NULL; }
	static void saveFrequencyTable( Buffer &outBuffer, const FrequencyTable &frequencyTable );
	static bool loadFrequencyTable( const Buffer &inBuffer, FrequencyTable &frequencyTable, Size nPositionOfFrequencyTable = 0 );
	static void destroyTree( HuffNode *&tree );

	bool open( const std::string &filename );
	void close( );

  public:
	Huffman( );
	virtual ~Huffman( );

	bool save( const std::string &outFile );

	Buffer &inBuffer( ) { return m_InBuffer; }
	Buffer &outBuffer( ) { return m_OutBuffer; }

	bool encode( const std::string &filename );
	bool decode( const std::string &filename );
	static bool encode( const Buffer &inBuffer,
					   Buffer &outBuffer );
	static bool decode( const Buffer &inBuffer,
					    Buffer &outBuffer );
};

} // end of namespace
#endif