pipeline {
    agent { label 'linux && amd64 && webrtc' }
    options {
        timeout(time: 300, unit: 'MINUTES')
        buildDiscarder(logRotator(numToKeepStr: '135', daysToKeepStr: '21'))
        gitLabConnection('GitLabConnectionJenkins')
    }
    stages {
        stage('Checkout SDK and MEGAchat'){
            steps {
                deleteDir()
                checkout scm
                dir('third-party/mega'){
                    sh "echo Cloning SDK branch ${SDK_BRANCH}"
                    checkout([
                        $class: 'GitSCM',
                        branches: [[name: "${SDK_BRANCH}"]],
                        userRemoteConfigs: [[ url: "git@code.developers.mega.co.nz:sdk/sdk.git", credentialsId: "12492eb8-0278-4402-98f0-4412abfb65c1" ]],
                        extensions: [
                            [$class: "UserIdentity",name: "jenkins", email: "jenkins@jenkins"]
                        ]
                    ])
                }
                script{
                    megachat_sources_workspace = WORKSPACE
                    sdk_sources_workspace = "${megachat_sources_workspace}/third-party/mega"
                }
            }
        }
        stage('Build MEGAchat'){
            environment{
                VCPKGPATH = "/opt/vcpkg"
                BUILD_DIR = "build_dir"
            }
            steps{
                dir(megachat_sources_workspace){
                    sh """
                        sed -i "s#MEGAChatTest#${env.USER_AGENT_TESTS}#g" tests/sdk_test/sdk_test.h
                    """
                    sh "echo Building SDK"
                    sh "cmake -DENABLE_CHATLIB_WERROR=ON -DUSE_WEBRTC=OFF -DCMAKE_BUILD_TYPE=${BUILD_TYPE}  -DVCPKG_ROOT=${VCPKGPATH} ${BUILD_OPTIONS} -DCMAKE_VERBOSE_MAKEFILE=ON \
                    -S ${megachat_sources_workspace} -B ${megachat_sources_workspace}/${BUILD_DIR}"
                    sh "cmake --build ${megachat_sources_workspace}/${BUILD_DIR} -j1"
                }
            }
        }
        stage('Run MEGAchat and SDK tests'){
            environment {
                MEGA_PWD0 = credentials('MEGA_PWD_DEFAULT')
                MEGA_PWD1 = credentials('MEGA_PWD_DEFAULT')
                MEGA_PWD2 = credentials('MEGA_PWD_DEFAULT')
                BUILD_DIR = "build_dir"
            }
            steps{
                script {
                    def lockLabel = ''
                    if ("${APIURL_TO_TEST}" == 'https://g.api.mega.co.nz/') {
                        lockLabel = 'SDK_Concurrent_Test_Accounts'
                    } else {
                        lockLabel = 'SDK_Concurrent_Test_Accounts_Staging'
                    }
                    lock(label: lockLabel, variable: 'ACCOUNTS_COMBINATION', quantity: 1, resource: null){
                        dir("${megachat_sources_workspace}/${BUILD_DIR}"){
                            script{
                                env.MEGA_EMAIL0 = "${env.ACCOUNTS_COMBINATION}"
                                echo "${env.ACCOUNTS_COMBINATION}"

                            }
                            sh """#!/bin/bash
                                set -x
                                ulimit -c unlimited
                                if [ -z \"${TESTS_PARALLEL}\" ]; then
                                    # Sequential run
                                    tests/sdk_test/megachat_tests --USERAGENT:${env.USER_AGENT_TESTS} --APIURL:${APIURL_TO_TEST} ${GTEST_FILTER} ${GTEST_REPEAT} &
                                    pid=\$!
                                    wait \$pid || FAILED=1
                                else
                                    # Parallel run
                                    tests/sdk_test/megachat_tests --USERAGENT:${env.USER_AGENT_TESTS} --APIURL:${APIURL_TO_TEST} ${GTEST_FILTER} ${GTEST_REPEAT} ${TESTS_PARALLEL} 2>&1 | tee tests.stdout
                                    [ \"\${PIPESTATUS[0]}\" != \"0\" ] && FAILED=1
                                fi

                                if [ -n "\$FAILED" ]; then
                                    if [ -z \"${TESTS_PARALLEL}\" ]; then
                                        # Sequential run
                                        coreFiles=\"test_pid_\$pid/core\"
                                    else
                                        # Parallel run
                                        procFailed=`grep \"<< PROCESS\" tests.stdout | sed 's/.*PID:\\([0-9]*\\).*/\\1/'`
                                        if [ -n \"\$procFailed\" ]; then
                                            for i in \$procFailed; do
                                                coreFiles=\"\$coreFiles test_pid_\$i/core\"
                                            done
                                        fi
                                    fi
                                fi

                                if [ -n \"\$coreFiles\" ]; then
                                    maxTime=10
                                    startTime=`date +%s`
                                    coresProcessed=0
                                    coresTotal=`echo \$coreFiles | wc -w`
                                    # While there are pending cores
                                    while [ \$coresProcessed -lt \$coresTotal ] && [ \$( expr `date +%s` - \$startTime ) -lt \$maxTime ]; do
                                        echo "Waiting for core dumps..."
                                        sleep 1
                                        for i in \$coreFiles; do
                                            if [ -e \"\$i\" ] && [ -z \"\$( lsof \$i 2>/dev/null )\" ]; then
                                                echo
                                                echo
                                                echo \"Processing core dump \$i :: \$(grep `echo \$i | sed 's#test_pid_\\([0-9].*\\)/core#\\1#'` tests.stdout)\"
                                                echo thread apply all bt > backtrace
                                                echo quit >> backtrace
                                                gdb -q tests/sdk_test/megachat_tests \$i -x backtrace
                                                tar rf core.tar \$i
                                                coresProcessed=`expr \$coresProcessed + 1`
                                                coreFiles=`echo \$coreFiles | sed -e \"s#\$i##\"`
                                            fi
                                        done
                                    done
                                    if [ -e core.tar ]; then
                                        tar rf core.tar -C tests/sdk_test/ megachat_tests
                                        gzip core.tar
                                    fi
                                fi

                                gzip -c test.log > test_${BUILD_ID}.log.gz || :
                                rm test.log || :
                                if [ ! -z "\$FAILED" ]; then
                                    false
                                fi
                            """
                        }
                    }
                }
            }
        }
    }
    post {
        always{
            archiveArtifacts artifacts: 'build_dir/test*.log*, build_dir/core.tar.gz', fingerprint: true
            deleteDir()
        }
    }
}