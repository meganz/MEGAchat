pipeline {
    agent { label 'osx && arm64' }
    options {
        timeout(time: 200, unit: 'MINUTES')
        buildDiscarder(logRotator(numToKeepStr: '135', daysToKeepStr: '21'))
        gitLabConnection('GitLabConnectionJenkins')
        ansiColor('xterm')
    }
    parameters {
        booleanParam(name: 'RESULT_TO_SLACK', defaultValue: true, description: 'Should the job result be sent to slack?')
        string(name: 'MEGACHAT_BRANCH', defaultValue: 'develop', description: 'Define a custom SDK branch.')
        string(name: 'SDK_BRANCH', defaultValue: 'develop', description: 'Define a custom SDK branch.')
        string(name: 'IOS_BRANCH', defaultValue: 'develop', description: 'Define a custom SDK branch.')

    }
    stages {
        stage('clean previous runs'){
            steps{
                deleteDir()
            }
        }
        stage('Checkout SDK MEGAchat and iOS'){
            steps {
                deleteDir()
                sh "echo Cloning iOS branch \"${params.IOS_BRANCH}\""
                checkout([
                    $class: 'GitSCM',
                    branches: [[name: "origin/${params.IOS_BRANCH}"]],
                    userRemoteConfigs: [[ url: "git@code.developers.mega.co.nz:mobile/ios/iOS_dev.git", credentialsId: "12492eb8-0278-4402-98f0-4412abfb65c1" ]],
                    extensions: [
                        [$class: "UserIdentity",name: "jenkins", email: "jenkins@jenkins"]
                    ]
                ])
                withCredentials([gitUsernamePassword(credentialsId: 'jenkins_sdk_token_with_user', gitToolName: 'Default')]) {
                    sh "git submodule update --init --recursive"
                }
                dir("Modules/DataSource/MEGAChatSDK/Sources/MEGAChatSDK"){
                    checkout([
                        $class: 'GitSCM',
                        branches: [[name: "origin/${params.MEGACHAT_BRANCH}"]],
                        userRemoteConfigs: [[ url: "git@code.developers.mega.co.nz:megachat/MEGAchat.git", credentialsId: "12492eb8-0278-4402-98f0-4412abfb65c1" ]],
                        extensions: [
                            [$class: "UserIdentity",name: "jenkins", email: "jenkins@jenkins"],
                        ]
                    ])
                }
                dir('Modules/Datasource/MEGASDK/Sources/MEGASDK'){
                    sh "echo Cloning megachat branch \"${params.SDK_BRANCH}\""
                    checkout([
                        $class: 'GitSCM',
                        branches: [[name: "origin/${params.SDK_BRANCH}"]],
                        userRemoteConfigs: [[ url: "git@code.developers.mega.co.nz:sdk/sdk.git", credentialsId: "12492eb8-0278-4402-98f0-4412abfb65c1" ]],
                        extensions: [
                            [$class: "UserIdentity",name: "jenkins", email: "jenkins@jenkins"]
                        ]
                    ])
                }
                script{
                    ios_sources_workspace = WORKSPACE
                    sdk_sources_workspace = "${ios_sources_workspace}/Modules/DataSource/MEGASDK/Sources/MEGASDK"
                    megachat_sources_workspace = "${ios_sources_workspace}/Modules/DataSource/MEGAChatSDK/Sources/MEGAChatSDK"
                }
            }
        }
        stage('Build MEGACHAT SDK and iOS'){
            environment{
                PATH = "/usr/local/bin:${env.PATH}"
                LIBTOOLIZE = "/usr/local/bin/glibtoolize"
            }
            steps{
                sh """
                    bundle config set --local path 'vendor/bundle'
                    bundle install
                """
                dir("${megachat_sources_workspace}/src"){
                    sh "cmake -P genDbSchema.cmake"
                }
                script {
                    sh "bundle exec fastlane configure_sdk_and_chat_library use_cache:true"
                }
                withCredentials([gitUsernamePassword(credentialsId: 'jenkins_sdk_token_with_user', gitToolName: 'Default')]) {
                    sh """
                        bundle exec fastlane update_plugins
                        bundle exec fastlane build_simulator
                    """
                }
            }
        }
    }
    post {
        always {
            script {
                if (params.RESULT_TO_SLACK) {
                    def sdk_commit = sh(script: "git -C ${sdk_sources_workspace} rev-parse HEAD", returnStdout: true).trim()
                    def megachat_commit = sh(script: "git -C ${megachat_sources_workspace} rev-parse HEAD", returnStdout: true).trim()
                    def messageStatus = currentBuild.currentResult
                    def messageColor = messageStatus == 'SUCCESS'? "#00FF00": "#FF0000" //green or red
                    def message = """
                        *MEGAchat iOS* <${BUILD_URL}|Build result>: '${messageStatus}'.
                        MEGAchat branch: `${MEGACHAT_BRANCH}`
                        MEGAchat commit: `${megachat_commit}`
                        SDK branch: `${SDK_BRANCH}`
                        SDK commit: `${sdk_commit}`
                    """.stripIndent()

                    withCredentials([string(credentialsId: 'slack_webhook_sdk_report', variable: 'SLACK_WEBHOOK_URL')]) {
                        sh """
                            curl -X POST -H 'Content-type: application/json' --data '
                                {
                                "attachments": [
                                    {
                                        "color": "${messageColor}",
                                        "blocks": [
                                        {
                                            "type": "section",
                                            "text": {
                                                    "type": "mrkdwn",
                                                    "text": "${message}"
                                            }
                                        }
                                        ]
                                    }
                                    ]
                                }' \${SLACK_WEBHOOK_URL}
                         """
                    }
                }
            }
            deleteDir() /* clean up our workspace */
        }
    }
}
// vim: syntax=groovy tabstop=4 shiftwidth=4
