/*
 * This file is part of the swblocks-baselib library.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <baselib/jni/JavaVirtualMachine.h>
#include <baselib/jni/JniEnvironment.h>
#include <baselib/jni/JniResourceWrappers.h>

#include <utests/baselib/Utf.h>
#include <utests/baselib/UtfArgsParser.h>

namespace
{
    using namespace bl::jni;

    class JniTestGlobalFixture
    {
    public:

        JniTestGlobalFixture()
        {
            ( void ) JavaVirtualMachine::instance();
        }

        ~JniTestGlobalFixture() NOEXCEPT
        {
            BL_NOEXCEPT_BEGIN()

            JavaVirtualMachine::destroy();

            BL_NOEXCEPT_END()
        }
    };

} // __unnamed

UTF_GLOBAL_FIXTURE( JniTestGlobalFixture )

UTF_AUTO_TEST_CASE( Jni_CreateJniEnvironments )
{
    using namespace bl;
    using namespace bl::jni;

    const auto createJniEnvironment = []( SAA_in const bool detachJniEnvAfterTest )
    {
        const auto& environment = JniEnvironment::instance();
        UTF_REQUIRE_EQUAL( environment.getVersion(), JNI_VERSION_1_8 );

        if( detachJniEnvAfterTest )
        {
            JniEnvironment::detach();
        }
    };

    createJniEnvironment( false /* detachJniEnvAfterTest */ );

    const int numThreads = 10;

    os::thread threads[ numThreads ];

    for( int i = 0; i < numThreads; ++i )
    {
        threads[i] = os::thread( createJniEnvironment, i % 2 == 0 /* detachJniEnvAfterTest */ );
    }

    for( int i = 0; i < numThreads; ++i )
    {
        threads[i].join();
    }
}

UTF_AUTO_TEST_CASE( Jni_LocalGlobalReferences )
{
    using namespace bl;
    using namespace bl::jni;

    const auto& environment = JniEnvironment::instance();

    const auto localReference = environment.findJavaClass( "java/lang/String" );
    UTF_REQUIRE( localReference.get() != nullptr );

    const auto globalReference = environment.createGlobalReference< jclass >( localReference );
    UTF_REQUIRE( globalReference.get() != nullptr );

    {
        /*
         * Verify local and global references in main and non main threads.
         */

        const auto verifyReferences = [ &localReference, &globalReference ]( SAA_in const bool isMainThread )
        {
            const auto& environment = JniEnvironment::instance();
            JNIEnv* jniEnv = environment.getRawPtr();

            UTF_REQUIRE_EQUAL(
                jniEnv -> GetObjectRefType( localReference.get() ),
                isMainThread
                    ? JNILocalRefType
                    : JNIInvalidRefType
                );

            UTF_REQUIRE_EQUAL( jniEnv -> GetObjectRefType( globalReference.get() ), JNIGlobalRefType );
        };

        verifyReferences( true /* isMainThread */ );

        bl::os::thread thread( verifyReferences, false /* isMainThread */ );
        thread.join();
    }
}

UTF_AUTO_TEST_CASE( Jni_JavaExceptions )
{
    using namespace bl::jni;

    const auto& environment = JniEnvironment::instance();

    UTF_CHECK_THROW_MESSAGE(
        environment.findJavaClass( "no/such/class" ),
        bl::JavaException,
        R"(Java class 'no/such/class' not found
Exception in thread "Thread-1" java.lang.NoClassDefFoundError: no/such/class
Caused by: java.lang.ClassNotFoundException: no.such.class
    at java.net.URLClassLoader.findClass(URLClassLoader.java:381)
    at java.lang.ClassLoader.loadClass(ClassLoader.java:424)
    at sun.misc.Launcher$AppClassLoader.loadClass(Launcher.java:331)
    at java.lang.ClassLoader.loadClass(ClassLoader.java:357))"
        );

    const auto threadClass = environment.findJavaClass( "java/lang/Thread" );

    const auto threadGetName = environment.getMethodID( threadClass.get(), "getName", "()Ljava/lang/String;" );
    UTF_REQUIRE( threadGetName != nullptr );

    UTF_CHECK_THROW_MESSAGE(
        environment.getMethodID( threadClass.get(), "foo", "()Ljava/lang/String;" ),
        bl::JavaException,
        "Method 'foo' with signature '()Ljava/lang/String;' not found in class 'java.lang.Thread'"
        );

    const auto threadCurrentThread = environment.getStaticMethodID( threadClass.get(), "currentThread", "()Ljava/lang/Thread;" );
    UTF_REQUIRE( threadCurrentThread != nullptr );

    UTF_CHECK_THROW_MESSAGE(
        environment.getStaticMethodID( threadClass.get(), "foo", "()Ljava/lang/Thread;" ),
        bl::JavaException,
        "Static method 'foo' with signature '()Ljava/lang/Thread;' not found in class 'java.lang.Thread'"
        );
}
