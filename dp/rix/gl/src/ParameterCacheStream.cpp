// Copyright NVIDIA Corporation 2013-2014
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//  * Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//  * Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//  * Neither the name of NVIDIA CORPORATION nor the names of its
//    contributors may be used to endorse or promote products derived
//    from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
// OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


#include <dp/rix/gl/inc/BufferGL.h>
#include <dp/rix/gl/inc/ParameterCacheStream.h>
#include <dp/rix/gl/inc/ParameterRendererUniform.h>
#include <dp/rix/gl/inc/ParameterRendererBuffer.h>
#include <dp/rix/gl/inc/ParameterRendererBufferAddressRange.h>
#include <dp/rix/gl/inc/ParameterRendererBufferDSA.h>
#include <dp/rix/gl/inc/ParameterRendererBufferRange.h>
#include <dp/util/Array.h>

namespace dp
{
  namespace rix
  {
    namespace gl
    {

      enum BufferMode
      {
        BM_BIND_BUFFER_RANGE,
        BM_BUFFER_SUBDATA
      };

      static const BufferMode BUFFER_MODE = BM_BIND_BUFFER_RANGE;
      //static const BufferMode BUFFER_MODE = BM_BUFFER_SUBDATA;

      /************************************************************************/
      /* ParameterCache                                                       */
      /************************************************************************/
      ParameterCache<ParameterCacheStream>::ParameterCache( ProgramPipelineGLHandle programPipeline, std::vector<ContainerDescriptorGLHandle> const &descriptors )
        : m_programPipeline( programPipeline )
        , m_descriptors( descriptors )
        , m_isBindlessUBOSupported(USE_UNIFORM_BUFFER_UNIFIED_MEMORY && !!glewGetExtension("GL_NV_uniform_buffer_unified_memory"))
      {
        m_uboDataUBO = dp::gl::Buffer::create();

        generateParameterStates( );
      }

      ParameterCache<ParameterCacheStream>::~ParameterCache( )
      {
      }

      void ParameterCache<ParameterCacheStream>::generateParameterStates(  )
      {
        DP_ASSERT( m_ubos.empty() );

        m_parameterStates.clear();

        size_t numDescriptors = m_descriptors.size();

        // assemble a vector of ParameterState objects that belong to variable containers
        for ( size_t i = 0; i < numDescriptors; ++i )
        {
          DP_ASSERT( m_programPipeline->m_programs.size() == 1 );

          ProgramGLHandle program = m_programPipeline->m_programs[0].get();
          ParameterState parameterState;
          parameterState.m_program             = program;
          parameterState.m_uniqueDescriptorIdx = i;
            
          ProgramGL::UniformInfos uniformInfos = program->getUniformInfos( m_descriptors[i] );
          ProgramGL::UniformInfos bufferVariables = program->getBufferInfos( m_descriptors[i] );

          if ( uniformInfos.size() && uniformInfos.begin()->second.blockIndex != -1 )
          {
            GLint blockIndex = uniformInfos.begin()->second.blockIndex;
            GLint binding;
            GLint blockSize;

            GLint programId = parameterState.m_program->getProgram()->getGLId();
            glGetActiveUniformBlockiv( programId, blockIndex, GL_UNIFORM_BLOCK_BINDING, &binding );
            glGetActiveUniformBlockiv( programId, blockIndex, GL_UNIFORM_BLOCK_DATA_SIZE, &blockSize);

            ParameterCacheEntryStreamBuffers parameterCacheEntries = createParameterCacheEntriesStreamBuffer( program, m_descriptors[i], uniformInfos );
            parameterState.m_numParameterObjects = parameterCacheEntries.size();

            switch ( BUFFER_MODE )
            {
            case BM_BUFFER_SUBDATA:
              {
                dp::gl::BufferSharedPtr ubo = dp::gl::Buffer::create();
                if ( glNamedBufferSubDataEXT /** GLEW_EXT_direct_state_access, glew has a bug not reporting this extension because the double versions are missing **/)
                {
                  parameterState.m_parameterRenderer.reset( new ParameterRendererBufferDSA( parameterCacheEntries, ubo, GL_UNIFORM_BUFFER, binding, 0, blockSize ) );
                }
                else
                {
                  parameterState.m_parameterRenderer.reset( new ParameterRendererBuffer( parameterCacheEntries, ubo, GL_UNIFORM_BUFFER, binding, 0, blockSize ) );
                }
                ubo->setData( GL_UNIFORM_BUFFER, blockSize, nullptr, GL_STREAM_DRAW );

                m_ubos.push_back( ubo );
                m_isUBOData.push_back(false);
              }
              break;
            case BM_BIND_BUFFER_RANGE:
              if ( m_isBindlessUBOSupported )
              {
                parameterState.m_parameterRenderer.reset( new ParameterRendererBufferAddressRange( parameterCacheEntries, m_uboDataUBO, GL_UNIFORM_BUFFER_ADDRESS_NV, binding, blockSize ) );
              }
              else
              {
                parameterState.m_parameterRenderer.reset( new ParameterRendererBufferRange( parameterCacheEntries, m_uboDataUBO, GL_UNIFORM_BUFFER, binding, blockSize ) );
              }

              m_isUBOData.push_back(true);
              break;
            default:
              DP_ASSERT(!"unsupported buffermode");
              m_isUBOData.push_back(false);
              break;
            }
          }
          else if ( bufferVariables.size() )
          {
            // TODO unify with UBO version
            GLint blockIndex = bufferVariables.begin()->second.blockIndex;
            GLint binding;
            GLint blockSize;

            GLint programId = parameterState.m_program->getProgram()->getGLId();

            GLenum props[] = { GL_BUFFER_BINDING, GL_BUFFER_DATA_SIZE };
            GLint results[sizeof dp::util::array(props)];
            glGetProgramResourceiv(program->getProgram()->getGLId(), GL_SHADER_STORAGE_BLOCK, blockIndex, sizeof dp::util::array(props), props, sizeof dp::util::array(results), NULL, results);
            binding = results[0];
            blockSize = results[1];

            ParameterCacheEntryStreamBuffers parameterCacheEntries = createParameterCacheEntriesStreamBuffer( program, m_descriptors[i], bufferVariables );
            parameterState.m_numParameterObjects = parameterCacheEntries.size();

            switch ( BUFFER_MODE )
            {
            case BM_BUFFER_SUBDATA:
              {
                dp::gl::BufferSharedPtr ubo = dp::gl::Buffer::create();
                if ( glNamedBufferSubDataEXT /** GLEW_EXT_direct_state_access, glew has a bug not reporting this extension because the double versions are missing **/)
                {
                  parameterState.m_parameterRenderer.reset( new ParameterRendererBufferDSA( parameterCacheEntries, ubo, GL_SHADER_STORAGE_BUFFER, binding, 0, blockSize ) );
                }
                else
                {
                  parameterState.m_parameterRenderer.reset( new ParameterRendererBuffer( parameterCacheEntries, ubo, GL_SHADER_STORAGE_BUFFER, binding, 0, blockSize ) );
                }
                ubo->setData( GL_UNIFORM_BUFFER, blockSize, nullptr, GL_STREAM_DRAW );

                m_ubos.push_back( ubo );
                m_isUBOData.push_back(false);
              }
              break;
            case BM_BIND_BUFFER_RANGE:
              parameterState.m_parameterRenderer.reset( new ParameterRendererBufferRange( parameterCacheEntries, m_uboDataUBO, GL_SHADER_STORAGE_BUFFER, binding, blockSize ) );
              m_isUBOData.push_back(true);
              break;
            default:
              DP_ASSERT(!"unsupported buffermode");
              m_isUBOData.push_back(false);
              break;
            }
          }
          else
          {
            ParameterCacheEntryStreams parameterCacheEntries = createParameterCacheEntryStreams( program, m_descriptors[i], m_isBindlessUBOSupported );
            parameterState.m_parameterRenderer.reset( new ParameterRendererUniform( parameterCacheEntries ) );
            parameterState.m_numParameterObjects = parameterCacheEntries.size();
            m_isUBOData.push_back(false);
          }

          // use ~0 as invalidate data ptr since this member is being used as offset to an ubo or ptr to data im memory
          // and 0 is a valid offset.
          parameterState.m_currentDataPtr = reinterpret_cast<void*>(~0);
          m_parameterStates.push_back( parameterState );
          m_dataSizes.push_back( parameterState.m_parameterRenderer->getCacheSize() );
        }
      }

      void ParameterCache<ParameterCacheStream>::renderParameters( ContainerCacheEntry const* containers )
      {
        ParameterRendererStream::CacheEntry const* cacheEntry = static_cast<ParameterRendererStream::CacheEntry const*>(containers);
        size_t const numParameterGroups = m_parameterStates.size();
        if ( !m_parameterStates.empty() )
        {
          // update shader uniforms
          for ( size_t idx = 0; idx < numParameterGroups;++idx )
          {
            if ( (m_parameterStates[idx].m_currentDataPtr != cacheEntry->m_containerData) )
            {
              m_parameterStates[idx].m_currentDataPtr = cacheEntry->m_containerData;

#if RIX_GL_SEPARATE_SHADER_OBJECTS_SUPPORT == 1
              glActiveShaderProgram( m_currentProgramPipeline->m_pipelineId, parameterState[idx].m_program->m_programId );
#endif
              m_parameterStates[idx].m_parameterRenderer->render( cacheEntry->m_containerData );
            } 
            ++cacheEntry;
          }
        }
      }

      void ParameterCache<ParameterCacheStream>::updateContainer( ContainerGLHandle container )
      {
        ContainerLocations::iterator it = m_containerLocations.find( container );
        if ( it != m_containerLocations.end() )
        {
          Location const& location = it->second; 

          // cannot use m_uniformData[offset] since dummy might be the last one which has size of 0
          // this would cause an out of bounds exception
          unsigned char *basePtr = (m_uniformData.empty() | m_isUBOData[location.m_descriptorIndex] ) ? nullptr : &m_uniformData[0];
          m_parameterStates[location.m_descriptorIndex].m_parameterRenderer->update( basePtr + location.m_offset, container->m_data );
          //location.m_dirty = false;
        }
      }

      void ParameterCache<ParameterCacheStream>::updateContainerCacheEntry( ContainerGLHandle container, ContainerCacheEntry* containerCacheEntry )
      {
        ParameterRendererStream::CacheEntry *cacheEntry = static_cast<ParameterRendererStream::CacheEntry*>(containerCacheEntry);
        Location & location = m_containerLocations[container];
        unsigned char *base = (m_uniformData.empty() | m_isUBOData[location.m_descriptorIndex] ) ? nullptr : &m_uniformData[0];
        size_t offset = location.m_offset;
        *cacheEntry = ParameterRendererStream::CacheEntry( base + offset );
      }

      void ParameterCache<ParameterCacheStream>::allocationBegin()
      {
        m_containerLocations.clear();
        m_currentUniformOffset = 0;
        m_currentUBOOffset = 0;
      }

      bool ParameterCache<ParameterCacheStream>::allocateContainer( ContainerGLHandle container, size_t descriptorIndex )
      {
        ContainerLocations::iterator it = m_containerLocations.find( container );
        if ( it  == m_containerLocations.end() )
        {
          size_t & offset = m_isUBOData[descriptorIndex] ? m_currentUBOOffset : m_currentUniformOffset;
          m_containerLocations[container] = Location(offset, descriptorIndex );
          offset += m_dataSizes[descriptorIndex];
          return true;
        }
        DP_ASSERT( it->second.m_descriptorIndex == descriptorIndex );
        return false;
      }

      void ParameterCache<ParameterCacheStream>::removeContainer( ContainerGLHandle container )
      {
        ContainerLocations::iterator it = m_containerLocations.find(const_cast<ContainerGLHandle>(container));
        if ( it != m_containerLocations.end() )
        {
          m_containerLocations.erase(it);
        }
      }

      void ParameterCache<ParameterCacheStream>::allocationEnd()
      {
        m_uniformData.resize( m_currentUniformOffset );
        m_uboDataUBO->setData( GL_UNIFORM_BUFFER, m_currentUBOOffset, nullptr, GL_STATIC_DRAW_ARB );
      }

      void ParameterCache<ParameterCacheStream>::resetParameterStateContainers()
      {
        for ( ParameterStates::iterator it = m_parameterStates.begin(); it != m_parameterStates.end(); ++it )
        {
          it->m_currentDataPtr = reinterpret_cast<void*>(~0);
        }
      }

      void ParameterCache<ParameterCacheStream>::activate()
      {
        size_t const numParameterGroups = m_parameterStates.size();
        // update shader uniforms
        for ( size_t idx = 0; idx < numParameterGroups;++idx )
        {
          m_parameterStates[idx].m_parameterRenderer->activate();
        }
      }


    } // namespace gl
  } // namespace rix
} // namespace dp
