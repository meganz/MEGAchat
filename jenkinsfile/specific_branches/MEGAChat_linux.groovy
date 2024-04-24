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
                WEBRTC_SRC="/home/jenkins/webrtc/src"
                PATH = "/home/jenkins/tools/depot_tools:${env.PATH}"
            }
            steps{
                dir(megachat_sources_workspace){
                    sh """
                        sed -i "s#MEGAChatTest#${env.USER_AGENT_TESTS}#g" tests/sdk_test/sdk_test.h
                        mkdir -p build
                    """
                }
                dir(sdk_sources_workspace){
                    sh """
                        ./autogen.sh
                        ./configure --disable-tests --enable-chat --enable-shared --without-pdfium --without-ffmpeg
                        sed -i "s#nproc#echo 1#" bindings/qt/build_with_webrtc.sh
                        cd bindings/qt && bash build_with_webrtc.sh all withExamples
                    """
                }

            }
        }
        stage('Run MEGAchat and SDK tests'){
            environment {
                MEGA_PWD0 = credentials('MEGA_PWD_DEFAULT')
                MEGA_PWD1 = credentials('MEGA_PWD_DEFAULT')
                MEGA_PWD2 = credentials('MEGA_PWD_DEFAULT')
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
                        dir("${megachat_sources_workspace}/build/subfolder"){
                            script{
                                env.MEGA_EMAIL0 = "${env.ACCOUNTS_COMBINATION}"
                                echo "${env.ACCOUNTS_COMBINATION}"
                            }
                            sh """#!/bin/bash
                                ulimit -c unlimited
                                ${megachat_sources_workspace}/build/MEGAchatTests/megachat_tests --USERAGENT:${env.USER_AGENT_TESTS} --APIURL:${APIURL_TO_TEST} ${TESTS_PARALLEL} 2>&1 | tee tests.stdout
                                [ \"\${PIPESTATUS[0]}\" != \"0\" ] && FAILED=1

                                if [ -n "\$FAILED" ]; then
                                    echo "Test failed with status \$FAILED"
                                    maxTime=10
                                    startTime=`date +%s`

                                    # Only a single core file can be handled, for either sequential or parallel run
                                    while [ \$( expr `date +%s` - \$startTime ) -lt \$maxTime ]; do
                                        if [ -e \"core\" ]; then
                                            echo "Processing core dump..."
                                            echo thread apply all bt > backtrace
                                            echo quit >> backtrace
                                            gdb -q ${megachat_sources_workspace}/build/MEGAchatTests/megachat_tests core -x ${megachat_sources_workspace}/build/subfolder/backtrace                                        
                                            tar chvzf core.tar.gz core megachat_tests
                                            break
                                        fi
                                        sleep 1
                                    done
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
        always {
            archiveArtifacts artifacts: "build/subfolder/test*.log*"
        }
    }
}
// vim: syntax=groovy tabstop=4 shiftwidth=4
