// Copyright NVIDIA Corporation 2014
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


#include <dp/rix/gl/inc/ParameterRendererBufferAddressRange.h>
#include <dp/rix/gl/inc/BufferGL.h>

namespace dp
{
  namespace rix
  {
    namespace gl
    {

      ParameterRendererBufferAddressRange::ParameterRendererBufferAddressRange()
      {
      }

      ParameterRendererBufferAddressRange::ParameterRendererBufferAddressRange( ParameterCacheEntryStreamBuffers const& parameterCacheEntries, dp::gl::BufferSharedPtr const& buffer, GLenum target, size_t bindingIndex, GLsizeiptr bindingLength )
        : m_parameters( parameterCacheEntries )
        , m_buffer( buffer )
        , m_target( target )
        , m_bindingIndex( GLint(bindingIndex) )
        , m_baseAddress( 0 )
        , m_bindingLength( bindingLength )
        , m_cacheData( new dp::util::Uint8[m_bindingLength] )
      {
        DP_ASSERT( !m_parameters.empty() );
      }

      void ParameterRendererBufferAddressRange::activate()
      {
        // update base address of buffer
        m_baseAddress = m_buffer->getAddress();
      }

      void ParameterRendererBufferAddressRange::render( void const* cache )
      { 
        GLsizeiptr const offset = reinterpret_cast<GLintptr>(cache);
        glBufferAddressRangeNV(m_target, m_bindingIndex,  m_baseAddress + offset, m_bindingLength);
      }

      void ParameterRendererBufferAddressRange::update( void* cache, void const* container )
      {
        // TODO cache is offset to m_ubo
        // keep temporary std::vector for data
        // TODO ensure that update is called only on non-empty containers
        size_t numParameters = m_parameters.size();
        if ( numParameters )
        {
          ParameterCacheEntryStreamBufferSharedPtr const* parameterObject = m_parameters.data();
          ParameterCacheEntryStreamBufferSharedPtr const* const parameterObjectEnd = parameterObject + numParameters;

          do
          {
            (*parameterObject)->update( m_cacheData.get(), container );
          } while (++parameterObject != parameterObjectEnd);

          m_buffer->setSubData( m_target, reinterpret_cast<size_t>(cache), m_bindingLength, m_cacheData.get() );
        }
        
      }

      size_t ParameterRendererBufferAddressRange::getCacheSize( ) const
      {
        // TODO determine alignment requirement of binding!
        return (m_bindingLength + 255) & ~255;
      }

    } // namespace gl
  } // namespace rix
} // namespace dp

